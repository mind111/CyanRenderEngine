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
    PackedGeometry* SceneRenderable::packedGeometry = nullptr;

    PackedGeometry::PackedGeometry(const Scene& scene)
    {
        for (auto meshInst : scene.meshInstances)
        {
            Mesh* mesh = meshInst->parent;
            auto entry = meshMap.find(mesh->name);
            if (entry == meshMap.end())
            {
                meshMap.insert({ mesh->name, mesh });
                meshes.push_back(mesh);
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

    SceneRenderable::SceneRenderable() {

    }

    u32 SceneRenderable::getMaterialID(MeshInstance* meshInstance, u32 submeshIndex) {
        std::unordered_map<std::string, u32> materialMap;

        if (meshInstance) {
            if (auto pbr = dynamic_cast<PBRMaterial*>(meshInstance->getMaterial(submeshIndex))) {
                auto matlEntry = materialMap.find(pbr->name);
                // create a material proxy for each unique material instance
                if (matlEntry == materialMap.end()) {
                    materialMap.insert({ pbr->name, materialBuffer->getNumElements() });
                    Material matlProxy = { };
                    matlProxy.kAlbedo = glm::vec4(pbr->parameter.kAlbedo, 1.f);
                    matlProxy.kMetallicRoughness = glm::vec4(pbr->parameter.kMetallic, pbr->parameter.kRoughness, 0.f, 0.f);
                    matlProxy.flags = glm::uvec4(pbr->parameter.getFlags());

                    // albedo
                    if (auto albedo = pbr->parameter.albedo) {
#if BINDLESS_TEXTURE
                        matlProxy.diffuseMapHandle = albedo->glHandle;
                        if (glIsTextureHandleResidentARB(matlProxy.diffuseMapHandle) == GL_FALSE) {
                            glMakeTextureHandleResidentARB(matlProxy.diffuseMapHandle);
                        }
#endif
                    }
                    // normal
                    if (auto normal = pbr->parameter.normal) {
#if BINDLESS_TEXTURE
                        matlProxy.normalMapHandle = normal->glHandle;
                        if (glIsTextureHandleResidentARB(matlProxy.normalMapHandle) == GL_FALSE)
                        {
                            glMakeTextureHandleResidentARB(matlProxy.normalMapHandle);
                        }
#endif
                    }
                    // metallicRoughness
                    if (auto metallicRoughness = pbr->parameter.metallicRoughness)
                    {
#if BINDLESS_TEXTURE
                        matlProxy.metallicRoughnessMapHandle = metallicRoughness->glHandle;
                        if (glIsTextureHandleResidentARB(matlProxy.metallicRoughnessMapHandle) == GL_FALSE)
                        {
                            glMakeTextureHandleResidentARB(matlProxy.metallicRoughnessMapHandle);
                        }
#endif
                    }
                    materialBuffer->addElement(matlProxy);
                    return materialBuffer->getNumElements() - 1;
                }
                else
                {
                    return matlEntry->second;
                }
            }
        }
    }

    SceneRenderable::SceneRenderable(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator) {
        viewBuffer = std::make_unique<ViewBuffer>();
        transformBuffer = std::make_unique<TransformBuffer>();
        instanceBuffer = std::make_unique<InstanceBuffer>();
        drawCallBuffer = std::make_unique<DrawCallBuffer>();
        materialBuffer = std::make_unique<MaterialBuffer>();
        directionalLightBuffer = std::make_unique<DirectionalLightBuffer>();

        if (!packedGeometry)
            packedGeometry = new PackedGeometry(*inScene);

        aabb = inScene->aabb;

        // todo: make this work with orthographic camera as well
        PerspectiveCamera* inCamera = dynamic_cast<PerspectiveCamera*>(inScene->camera->getCamera());
        if (inCamera) {
            camera.eye = inCamera->position;
            camera.lookAt = inCamera->lookAt;
            camera.right = inCamera->right();
            camera.forward = inCamera->forward();
            camera.up = inCamera->up();
            camera.n = inCamera->n;
            camera.f = inCamera->f;
            camera.fov = inCamera->fov;
            camera.aspect = inCamera->aspectRatio;
            camera.view = inCamera->view();
            camera.projection = inCamera->projection();
        }

        // build list of mesh instances, transforms, and lights
        for (auto entity : sceneView.entities)
        {
            auto lightComponents = entity->getComponent<ILightComponent>();
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

    void SceneRenderable::clone(SceneRenderable& dst, const SceneRenderable& src) {
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
    SceneRenderable::SceneRenderable(const SceneRenderable& src) {
        clone(*this, src);
    }

    SceneRenderable& SceneRenderable::operator=(const SceneRenderable& src) {
        clone(*this, src);
        return *this;
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void SceneRenderable::upload() {
#define VIEW_BUFFER_BINDING 0
#define TRANSFORM_BUFFER_BINDING 1
#define INSTANCE_DESC_BUFFER_BINDING 2
#define SUBMESH_BUFFER_BINDING 3
#define VERTEX_BUFFER_BINDING 4
#define INDEX_BUFFER_BINDING 5
#define DRAWCALL_BUFFER_BINDING 6
#define MATERIAL_BUFFER_BINDING 7
#define DIRECTIONALLIGHT_BUFFER_BINDING 8

        packedGeometry->vertexBuffer.bind(VERTEX_BUFFER_BINDING);
        packedGeometry->indexBuffer.bind(INDEX_BUFFER_BINDING);
        packedGeometry->submeshes.bind(SUBMESH_BUFFER_BINDING);

        // view
        viewBuffer->data.constants.view = camera.view;
        viewBuffer->data.constants.projection = camera.projection;
        viewBuffer->upload();
        viewBuffer->bind(VIEW_BUFFER_BINDING);
        // transform
        transformBuffer->upload();
        transformBuffer->bind(TRANSFORM_BUFFER_BINDING);
        // instance
        instanceBuffer->upload();
        instanceBuffer->bind(INSTANCE_DESC_BUFFER_BINDING);
        // material
        materialBuffer->upload();
        materialBuffer->bind(MATERIAL_BUFFER_BINDING);
        drawCallBuffer->upload();
        drawCallBuffer->bind(DRAWCALL_BUFFER_BINDING);
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
        directionalLightBuffer->bind(DIRECTIONALLIGHT_BUFFER_BINDING);
    }
}