#include <unordered_map>

#include "Lights.h"
#include "RenderableScene.h"
#include "Camera.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponents.h"
#include "GpuLights.h"
#include "AssetManager.h"

namespace Cyan
{
    RenderableScene::Camera::Camera(const PerspectiveCamera& inCamera)
    {
        eye = inCamera.position;
        lookAt = inCamera.lookAt;
        right = inCamera.right();
        forward = inCamera.forward();
        up = inCamera.up();
        n = inCamera.n;
        f = inCamera.f;
        fov = inCamera.fov;
        aspect = inCamera.aspectRatio;
        view = inCamera.view();
        projection = inCamera.projection();
    }

    RenderableScene::RenderableScene() 
    {

    }

    RenderableScene::RenderableScene(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator) 
    {
        viewBuffer = std::make_unique<ViewBuffer>("ViewBuffer");
        transformBuffer = std::make_unique<TransformBuffer>("TransformBuffer");
        instanceBuffer = std::make_unique<InstanceBuffer>("InstanceBuffer");
        drawCallBuffer = std::make_unique<DrawCallBuffer>("DrawCallBuffer");
        directionalLightBuffer = std::make_unique<DirectionalLightBuffer>("DirectionalLightBuffer");

        aabb = inScene->m_aabb;

        // todo: make this work with orthographic camera as well
        PerspectiveCamera* inCamera = dynamic_cast<PerspectiveCamera*>(inScene->m_mainCamera->getCamera());
        camera = Camera(*inCamera);

        // build list of mesh instances, transforms, and lights
        for (auto entity : sceneView.entities)
        {
            auto lightComponents = entity->getComponents<ILightComponent>();
            if (!lightComponents.empty())
            {
                for (auto lightComponent : lightComponents)
                {
                    if (std::string(lightComponent->getTag()) == std::string("DirectionalLightComponent")) {
                        auto directionalLightComponent = dynamic_cast<DirectionalLightComponent*>(lightComponent);
                        assert(directionalLightComponent);
                        auto directionalLight = directionalLightComponent->directionalLight.get();
                        if (directionalLight) 
                        {
                            directionalLights.push_back(directionalLight);

                            if (auto csmDirectionalLight = dynamic_cast<CSMDirectionalLight*>(directionalLight)) 
                            {
                                directionalLightBuffer->addElement(csmDirectionalLight->buildGpuLight());
                            }
                            else if (auto basicDirectionalLight = dynamic_cast<DirectionalLight*>(directionalLight)) 
                            {
                                assert(0);
                            }
                        }
                    }
                    else if (std::string(lightComponent->getTag()) == std::string("PointLightComponent")) 
                    {

                    }
                }
            }

            // static meshes
            if (auto staticMesh = dynamic_cast<StaticMeshEntity*>(entity))
            {
                meshInstances.push_back(staticMesh->getMeshInstance());
                glm::mat4 model = staticMesh->getWorldTransformMatrix();
                transformBuffer->addElement(model);
            }
        }

        // build instance descriptors
        u32 materialCount = 0;
        for (u32 i = 0; i < meshInstances.size(); ++i) 
        {
            auto mesh = meshInstances[i]->mesh;
            for (u32 sm = 0; sm < mesh->numSubmeshes(); ++sm) 
            {
                auto submesh = mesh->getSubmesh(sm);
                auto smDesc = StaticMesh::getSubmeshDesc(submesh);
                // todo: properly handle other types of geometries
                if (dynamic_cast<Triangles*>(submesh->geometry)) 
                {
                    InstanceDesc desc = { };
                    desc.submesh = submesh->index;
                    desc.transform = i;
                    desc.material = materialCount++;
                    instanceBuffer->addElement(desc);
                }
            }
        }

        // organize instance descriptors
        if (instanceBuffer->getNumElements() > 0) 
        {
            struct InstanceDescSortKey 
            {
                inline bool operator() (const InstanceDesc& lhs, const InstanceDesc& rhs) 
                {
                    return (lhs.submesh < rhs.submesh);
                }
            };
            // todo: this sort maybe too slow at some point 
            // sort the instance buffer to group submesh instances that share the same submesh right next to each other
            std::sort(instanceBuffer->data.array.begin(), instanceBuffer->data.array.end(), InstanceDescSortKey());

            // build a buffer to store the instance index for each draw
            drawCallBuffer->addElement(0);
            u32 prev = (*instanceBuffer)[0].submesh;
            for (u32 i = 1; i < instanceBuffer->getNumElements(); ++i)
            {
                if (instanceBuffer->data.array[i].submesh != prev)
                {
                    drawCallBuffer->addElement(i);
                }
                prev = (*instanceBuffer)[i].submesh;
            }
            drawCallBuffer->addElement(instanceBuffer->getNumElements());
        }

        skybox = inScene->skybox;
        skyLight = inScene->skyLight;
    }

    void RenderableScene::clone(const RenderableScene& src) 
    {
        aabb = src.aabb;
        camera = src.camera;
        meshInstances = src.meshInstances;
        viewBuffer = std::unique_ptr<ViewBuffer>(src.viewBuffer->clone());
        transformBuffer = std::unique_ptr<TransformBuffer>(src.transformBuffer->clone());
        instanceBuffer = std::unique_ptr<InstanceBuffer>(src.instanceBuffer->clone());
        drawCallBuffer = std::unique_ptr<DrawCallBuffer>(src.drawCallBuffer->clone());
        skybox = src.skybox;
        skyLight = src.skyLight;
        directionalLights = src.directionalLights;
        directionalLightBuffer = std::unique_ptr<DirectionalLightBuffer>(src.directionalLightBuffer->clone());

        onClone(src);
    }

    /**
    * the copy constructor performs a deep copy instead of simply copying over
    * pointers / references
    */
    RenderableScene::RenderableScene(const RenderableScene& src) 
    {
        clone(src);
    }


    RenderableScene& RenderableScene::operator=(const RenderableScene& src) 
    {
        clone(src);
        return *this;
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void RenderableScene::upload() 
    {
        auto gfxc = Renderer::get()->getGfxCtx();
        // todo: doing this every frame is kind of redundant, maybe to a message kind of thing here to only trigger update when there is actually an update
        // to these global buffers?
        auto& submeshBuffer = StaticMesh::getSubmeshBuffer();
        submeshBuffer.upload();
        auto& gVertexBuffer = StaticMesh::getGlobalVertexBuffer();
        gVertexBuffer.upload();
        auto& gIndexBuffer = StaticMesh::getGlobalIndexBuffer();
        gIndexBuffer.upload();

        gfxc->setShaderStorageBuffer(&submeshBuffer);
        gfxc->setShaderStorageBuffer(&gVertexBuffer);
        gfxc->setShaderStorageBuffer(&gIndexBuffer);

        // view
        viewBuffer->data.constants.view = camera.view;
        viewBuffer->data.constants.projection = camera.projection;
        viewBuffer->upload();
        gfxc->setShaderStorageBuffer(viewBuffer.get());

        transformBuffer->upload();
        gfxc->setShaderStorageBuffer(transformBuffer.get());

        instanceBuffer->upload();
        gfxc->setShaderStorageBuffer(instanceBuffer.get());

        drawCallBuffer->upload();
        gfxc->setShaderStorageBuffer(drawCallBuffer.get());

        // directional lights
        for (i32 i = 0; i < directionalLightBuffer->getNumElements(); ++i) {
            for (i32 j = 0; j < CascadedShadowMap::kNumCascades; ++j) {
                auto handle = (*directionalLightBuffer)[i].cascades[j].shadowMap.depthMapHandle;
                if (glIsTextureHandleResidentARB(handle) == GL_FALSE) {
                    glMakeTextureHandleResidentARB(handle);
                }
            }
        }
        directionalLightBuffer->upload();
        gfxc->setShaderStorageBuffer(directionalLightBuffer.get());

        onUpload();
    }

    RenderableSceneBindless::RenderableSceneBindless(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
        : RenderableScene(inScene, sceneView, allocator)
    {
        materialBuffer = std::make_unique<MaterialBuffer>("MaterialBuffer");
        for (i32 i = 0; i < meshInstances.size(); ++i)
        {
            auto mesh = meshInstances[i]->mesh;
            for (i32 sm = 0; sm < mesh->numSubmeshes(); ++sm)
            {
                Material* matl = meshInstances[i]->getMaterial(sm);
                materialBuffer->addElement(matl->buildGpuMaterial());
            }
        }
    }

    RenderableSceneBindless::RenderableSceneBindless(const RenderableScene& src)
    {
        clone(src);
    }

    void RenderableSceneBindless::onClone(const RenderableScene& src)
    {
        const RenderableSceneBindless* srcPtr = dynamic_cast<const RenderableSceneBindless*>(&src);
        if (srcPtr)
        {
            materialBuffer = std::unique_ptr<MaterialBuffer>(srcPtr->materialBuffer->clone());
        }
    }

    void RenderableSceneBindless::onUpload()
    {
        auto gfxc = GfxContext::get();
        materialBuffer->upload();
        gfxc->setShaderStorageBuffer(materialBuffer.get());
    }

    RenderableSceneTextureAtlas::RenderableSceneTextureAtlas(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
        : RenderableScene(inScene, sceneView, allocator)
    {
        auto assetManager = AssetManager::get();

        subtextureBufferR8 = std::make_unique<SubtextureBuffer>("SubtextureBuffer_R8");
        subtextureBufferR8->data.array.insert(subtextureBufferR8->data.array.end(), assetManager->atlases[(u32)Texture2DAtlas::Format::kR8]->subtextures.begin(), assetManager->atlases[(u32)Texture2DAtlas::Format::kR8]->subtextures.end());
        imageTransformBufferR8 = std::make_unique<ImageTransformBuffer>("ImageTransformBuffer_R8");
        imageTransformBufferR8->data.array.insert(imageTransformBufferR8->data.array.end(), assetManager->atlases[(u32)Texture2DAtlas::Format::kR8]->imageTransforms.begin(), assetManager->atlases[(u32)Texture2DAtlas::Format::kR8]->imageTransforms.end());

        subtextureBufferRGB8 = std::make_unique<SubtextureBuffer>("SubtextureBuffer_RGB8");
        subtextureBufferRGB8->data.array.insert(subtextureBufferRGB8->data.array.end(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGB8]->subtextures.begin(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGB8]->subtextures.end());
        imageTransformBufferRGB8 = std::make_unique<ImageTransformBuffer>("ImageTransformBuffer_RGB8");
        imageTransformBufferRGB8->data.array.insert(imageTransformBufferRGB8->data.array.end(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGB8]->imageTransforms.begin(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGB8]->imageTransforms.end());

        subtextureBufferRGBA8 = std::make_unique<SubtextureBuffer>("SubtextureBuffer_RGBA8");
        subtextureBufferRGBA8->data.array.insert(subtextureBufferRGBA8->data.array.end(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGBA8]->subtextures.begin(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGBA8]->subtextures.end());
        imageTransformBufferRGBA8 = std::make_unique<ImageTransformBuffer>("ImageTransformBuffer_RGBA8");
        imageTransformBufferRGBA8->data.array.insert(imageTransformBufferRGBA8->data.array.end(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGBA8]->imageTransforms.begin(), assetManager->atlases[(u32)Texture2DAtlas::Format::kRGBA8]->imageTransforms.end());
    }

    RenderableSceneTextureAtlas::RenderableSceneTextureAtlas(const RenderableScene& src)
    {
        clone(src);
    }

    void RenderableSceneTextureAtlas::onClone(const RenderableScene& src)
    {
        const RenderableSceneTextureAtlas* srcPtr = dynamic_cast<const RenderableSceneTextureAtlas*>(&src);
        if (srcPtr)
        {
            // materialBuffer = std::unique_ptr<MaterialBuffer>(srcPtr->materialBuffer->clone());
        }
    }

    void RenderableSceneTextureAtlas::onUpload()
    {
        auto assetManager = AssetManager::get();
        auto gfxc = GfxContext::get();

        imageTransformBufferR8->upload();
        subtextureBufferR8->upload();
        gfxc->setShaderStorageBuffer(imageTransformBufferR8.get());
        gfxc->setShaderStorageBuffer(subtextureBufferR8.get());

        imageTransformBufferRGB8->upload();
        subtextureBufferRGB8->upload();
        gfxc->setShaderStorageBuffer(imageTransformBufferRGB8.get());
        gfxc->setShaderStorageBuffer(subtextureBufferRGB8.get());

        imageTransformBufferRGBA8->upload();
        subtextureBufferRGBA8->upload();
        gfxc->setShaderStorageBuffer(imageTransformBufferRGBA8.get());
        gfxc->setShaderStorageBuffer(subtextureBufferRGBA8.get());
    }
}
