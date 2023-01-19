#include <unordered_map>

#include "Lights.h"
#include "RenderableScene.h"
#include "Camera.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponents.h"
#include "GpuLights.h"

namespace Cyan
{
    PackedGeometry* RenderableScene::packedGeometry = nullptr;

    PackedGeometry::PackedGeometry(const Scene& scene) 
        : vertexBuffer("VertexBuffer")
        , indexBuffer("IndexBuffer") 
        , submeshes("SubmeshBuffer") 
    {
        for (auto e : scene.m_entities)
        {
            if (StaticMeshEntity* staticMeshEntity = dynamic_cast<StaticMeshEntity*>(e))
            {
                Mesh* mesh = staticMeshEntity->getMeshInstance()->parent;
                auto entry = meshMap.find(mesh->name);
                if (entry == meshMap.end())
                {
                    meshMap.insert({ mesh->name, mesh });
                    meshes.push_back(mesh);
                }
            }
        }

        // build unified vertex buffer and index buffer
        for (auto mesh : meshes)
        {
            auto entry = submeshMap.find(mesh->name);
            if (entry == submeshMap.end())
                submeshMap.insert({ mesh->name, submeshes.data.array.size()});

            for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
            {
                auto sm = mesh->getSubmesh(i);
                if (auto triSubmesh = dynamic_cast<Mesh::Submesh<Triangles>*>(sm))
                {
                    auto& vertices = triSubmesh->getVertices();
                    auto& indices = triSubmesh->getIndices();

                    submeshes.data.array.push_back(
                        {
                            /*baseVertex=*/(u32)vertexBuffer.data.array.size(),
                            /*baseIndex=*/(u32)indexBuffer.data.array.size(),
                            /*numVertices=*/(u32)vertices.size(),
                            /*numIndices=*/(u32)indices.size()
                        }
                    );

                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        vertexBuffer.data.array.emplace_back();
                        Vertex& vertex = vertexBuffer.data.array.back();
                        vertex.pos = glm::vec4(vertices[v].pos, 1.f);
                        vertex.normal = glm::vec4(vertices[v].normal, 0.f);
                        vertex.tangent = vertices[v].tangent;
                        vertex.texCoord = glm::vec4(vertices[v].texCoord0, vertices[v].texCoord1);
                    }
                    for (u32 ii = 0; ii < indices.size(); ++ii)
                    {
                        indexBuffer.addElement(indices[ii]);
                    }
                }
            }
        }

        vertexBuffer.upload();
        indexBuffer.upload();
        submeshes.upload();
    }

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

    u32 RenderableScene::getMaterialID(MeshInstance* meshInstance, u32 submeshIndex) 
    {
        std::unordered_map<std::string, u32> materialMap;

        if (meshInstance) {
            if (auto matl = meshInstance->getMaterial(submeshIndex)) {
                auto matlEntry = materialMap.find(matl->name);
                if (matlEntry == materialMap.end()) {
                    materialMap.insert({ matl->name, materialBuffer->getNumElements() });
                    materialBuffer->addElement(matl->buildGpuMaterial());
                    return materialBuffer->getNumElements() - 1;
                }
                else {
                    return matlEntry->second;
                }
            }
        }
    }

    RenderableScene::RenderableScene(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator) 
    {
        viewBuffer = std::make_unique<ViewBuffer>("ViewBuffer");
        transformBuffer = std::make_unique<TransformBuffer>("TransformBuffer");
        instanceBuffer = std::make_unique<InstanceBuffer>("InstanceBuffer");
        drawCallBuffer = std::make_unique<DrawCallBuffer>("DrawCallBuffer");
        materialBuffer = std::make_unique<MaterialBuffer>("MaterialBuffer");
        directionalLightBuffer = std::make_unique<DirectionalLightBuffer>("DirectionalLightBuffer");

        if (!packedGeometry)
            packedGeometry = new PackedGeometry(*inScene);

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
                        if (directionalLight) {
                            directionalLights.push_back(directionalLight);

                            if (auto csmDirectionalLight = dynamic_cast<CSMDirectionalLight*>(directionalLight)) {
                                directionalLightBuffer->addElement(csmDirectionalLight->buildGpuLight());
                            }
                            else if (auto basicDirectionalLight = dynamic_cast<DirectionalLight*>(directionalLight)) {
                                assert(0);
                            }
                        }
                    }
                    else if (std::string(lightComponent->getTag()) == std::string("PointLightComponent")) {

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
        for (u32 i = 0; i < meshInstances.size(); ++i) {
            auto mesh = meshInstances[i]->parent;
            auto entry = packedGeometry->submeshMap.find(mesh->name);
            u32 baseSubmesh = 0;
            if (entry != packedGeometry->submeshMap.end()) {
                baseSubmesh = entry->second;
            }
            else {
                cyanError("Failed to find mesh %s in packed geoemtry buffer", mesh->name);
                assert(0);
            }
            for (u32 sm = 0; sm < mesh->numSubmeshes(); ++sm) {
                InstanceDesc desc = { };
                desc.submesh = baseSubmesh + sm;
                desc.transform = i;
                desc.material = getMaterialID(meshInstances[i], sm);
                instanceBuffer->addElement(desc);
            }
        }

        // organize instance descriptors
        if (instanceBuffer->getNumElements() > 0) {
            struct InstanceDescSortKey {
                inline bool operator() (const InstanceDesc& lhs, const InstanceDesc& rhs) {
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

        // build lighting data
        skybox = inScene->skybox;
        skyLight = inScene->skyLight;
    }

    void RenderableScene::clone(RenderableScene& dst, const RenderableScene& src) 
    {
        dst.aabb = src.aabb;
        dst.camera = src.camera;
        dst.meshInstances = src.meshInstances;
        dst.viewBuffer = std::unique_ptr<ViewBuffer>(src.viewBuffer->clone());
        dst.transformBuffer = std::unique_ptr<TransformBuffer>(src.transformBuffer->clone());
        dst.instanceBuffer = std::unique_ptr<InstanceBuffer>(src.instanceBuffer->clone());
        dst.drawCallBuffer = std::unique_ptr<DrawCallBuffer>(src.drawCallBuffer->clone());
        dst.materialBuffer = std::unique_ptr<MaterialBuffer>(src.materialBuffer->clone());
        dst.skybox = src.skybox;
        dst.skyLight = src.skyLight;
        dst.directionalLights = src.directionalLights;
        dst.directionalLightBuffer = std::unique_ptr<DirectionalLightBuffer>(src.directionalLightBuffer->clone());
    }

    /**
    * the copy constructor performs a deep copy instead of simply copying over
    * pointers / references
    */
    RenderableScene::RenderableScene(const RenderableScene& src) {
        clone(*this, src);
    }

    RenderableScene& RenderableScene::operator=(const RenderableScene& src) {
        clone(*this, src);
        return *this;
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void RenderableScene::upload() 
    {
        auto gfxc = Renderer::get()->getGfxCtx();
        gfxc->setShaderStorageBuffer(&packedGeometry->vertexBuffer);
        gfxc->setShaderStorageBuffer(&packedGeometry->indexBuffer);
        gfxc->setShaderStorageBuffer(&packedGeometry->submeshes);
        // view
        viewBuffer->data.constants.view = camera.view;
        viewBuffer->data.constants.projection = camera.projection;
        viewBuffer->upload();
        gfxc->setShaderStorageBuffer(viewBuffer.get());

        transformBuffer->upload();
        gfxc->setShaderStorageBuffer(transformBuffer.get());

        instanceBuffer->upload();
        gfxc->setShaderStorageBuffer(instanceBuffer.get());

        materialBuffer->upload();
        gfxc->setShaderStorageBuffer(materialBuffer.get());

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
    }
}