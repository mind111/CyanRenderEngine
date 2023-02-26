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

    RenderableScene::RenderableScene(const Scene* inScene, const SceneView& sceneView) 
    {
        // todo: this shouldn't need to be reconstructed every frame, they should only need to be updated when there is any size changes, but maybe they
        // are not as inefficient as I thought they would be, so opt for destroying and re-creating every frame for now until they cause issues. 
        viewBuffer = std::make_unique<ViewBuffer>("ViewBuffer");
        transformBuffer = std::make_unique<TransformBuffer>("TransformBuffer");
        instanceBuffer = std::make_unique<InstanceBuffer>("InstanceBuffer");
        drawCallBuffer = std::make_unique<DrawCallBuffer>("DrawCallBuffer");
        directionalLightBuffer = std::make_unique<DirectionalLightBuffer>("DirectionalLightBuffer");

        aabb = inScene->m_aabb;

        // todo: make this work with orthographic camera as well
        camera = Camera(sceneView.camera);

        // build list of mesh instances, transforms, and lights
        for (auto entity : inScene->m_entities)
        {
            // static meshes
            if (auto staticMesh = dynamic_cast<StaticMeshEntity*>(entity))
            {
                auto mesh = staticMesh->getMeshInstance()->mesh;
                meshInstances.push_back(staticMesh->getMeshInstance());
                glm::mat4 model = staticMesh->getWorldTransformMatrix();
                transformBuffer->addElement(model);
            }

            // lights
            const auto& lightComponents = entity->getComponents<ILightComponent>();
            if (!lightComponents.empty())
            {
                for (auto lightComponent : lightComponents)
                {
                    if (std::string(lightComponent->getTag()) == std::string("DirectionalLightComponent")) 
                    {
                        auto directionalLightComponent = static_cast<DirectionalLightComponent*>(lightComponent);
                        auto directionalLight = directionalLightComponent->directionalLight.get();
                        if (directionalLight) 
                        {
                            directionalLights.push_back(directionalLight);
                            // todo: this is slightly awkward
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
        }

        // build instance descriptors
        u32 materialCount = 0;
        for (u32 i = 0; i < meshInstances.size(); ++i) 
        {
            auto mesh = meshInstances[i]->mesh;
            for (u32 sm = 0; sm < mesh->numSubmeshes(); ++sm) 
            {
                auto submesh = mesh->getSubmesh(sm);
                if (submesh->bInitialized)
                {
                    // todo: properly handle other types of geometries
                    if (dynamic_cast<Triangles*>(submesh->geometry.get())) 
                    {
                        InstanceDesc desc = { };
                        desc.submesh = submesh->index;
                        desc.transform = i;
                        desc.material = materialCount++;
                        instanceBuffer->addElement(desc);
                    }
                }
            }
        }

        // todo: need to implement IndirectDrawBuffer and then the following part becomes building that indirect draw buffer
        // organize instance descriptors and build a drawcall buffer
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

        // todo: these two need to be turned into entities
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
        // todo: this is only a simple hack for preventing re-uploading same data go gpu multiple times during a frame, this works if assuming
        // that RenderableScene is immutable once constructed, or else doing it this way may prevent someone to edit/update a RenderableScene
        // object's data while that modification is not reflected because of this hack!
        if (!bUploaded)
        {
            auto gfxc = Renderer::get()->getGfxCtx();

            auto& submeshBuffer = StaticMesh::getSubmeshBuffer();
            auto& gVertexBuffer = StaticMesh::getGlobalVertexBuffer();
            auto& gIndexBuffer = StaticMesh::getGlobalIndexBuffer();
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
            for (i32 i = 0; i < directionalLightBuffer->getNumElements(); ++i) 
            {
                for (i32 j = 0; j < CascadedShadowMap::kNumCascades; ++j) 
                {
                    auto handle = (*directionalLightBuffer)[i].cascades[j].shadowMap.depthMapHandle;
                    if (glIsTextureHandleResidentARB(handle) == GL_FALSE) 
                    {
                        glMakeTextureHandleResidentARB(handle);
                    }
                }
            }
            directionalLightBuffer->upload();
            gfxc->setShaderStorageBuffer(directionalLightBuffer.get());

            onUpload();

            bUploaded = true;
        }
    }

    RenderableSceneBindless::RenderableSceneBindless(const Scene* inScene, const SceneView& sceneView)
        : RenderableScene(inScene, sceneView)
    {
        materialBuffer = std::make_unique<MaterialBuffer>("MaterialBuffer");
        for (i32 i = 0; i < meshInstances.size(); ++i)
        {
            auto mesh = meshInstances[i]->mesh;
            for (i32 sm = 0; sm < mesh->numSubmeshes(); ++sm)
            {
                MaterialBindless* matl = static_cast<MaterialBindless*>(meshInstances[i]->getMaterial(sm));
                materialBuffer->addElement(matl->buildGpuData());
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

    RenderableSceneTextureAtlas::RenderableSceneTextureAtlas(const Scene* inScene, const SceneView& sceneView)
        : RenderableScene(inScene, sceneView)
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
