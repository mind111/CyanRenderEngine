#include <unordered_map>

#include "RenderableScene.h"
#include "Camera.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponent.h"
#include "LightRenderable.h"

namespace Cyan
{
    PackedGeometry* RenderableScene::packedGeometry = nullptr;

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
                submeshMap.insert({ mesh->name, submeshes.ssboStruct.dynamicArray.size()});

            for (u32 i = 0; i < mesh->numSubmeshes(); ++i)
            {
                auto sm = mesh->getSubmesh(i);
                if (auto triSubmesh = dynamic_cast<Mesh::Submesh<Triangles>*>(sm))
                {
                    auto& vertices = triSubmesh->getVertices();
                    auto& indices = triSubmesh->getIndices();

                    submeshes.ssboStruct.dynamicArray.push_back(
                        {
                            /*baseVertex=*/(u32)vertexBuffer.ssboStruct.dynamicArray.size(),
                            /*baseIndex=*/(u32)indexBuffer.ssboStruct.dynamicArray.size(),
                            /*numVertices=*/(u32)vertices.size(),
                            /*numIndices=*/(u32)indices.size()
                        }
                    );

                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        vertexBuffer.ssboStruct.dynamicArray.emplace_back();
                        Vertex& vertex = vertexBuffer.ssboStruct.dynamicArray.back();
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

        vertexBuffer.update();
        indexBuffer.update();
        submeshes.update();
    }

    RenderableScene::RenderableScene(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
        : camera{ sceneView.camera->view(), sceneView.camera->projection() }
    {
        viewSsbo = std::make_shared<ViewSsbo>();
        transformSsbo = std::make_shared<TransformSsbo>();
        instances = std::make_shared<InstanceBuffer>();
        drawCalls = std::make_shared<DrawCallBuffer>();
        materials = std::make_shared<MaterialBuffer>();

        // build list of mesh instances, transforms, and lights
        for (auto entity : sceneView.entities)
        {
            auto lightComponents = entity->getComponent<ILightComponent>();
            if (!lightComponents.empty())
            {
                for (auto lightComponent : lightComponents)
                {
                    /* note:
                        using a custom deleter since `allocator` used in this case is using placement new, need to explicitly invoke
                        destructor to avoid memory leaking as the object being constructed using placement new might heap allocate its members.
                        Thus, need to invoke destructor to make sure those heap allocated members are released, no need to free memory since the allocator's
                        memory will be reset and reused each frame.
                    */
                    lights.push_back(std::shared_ptr<ILightRenderable>(lightComponent->buildRenderableLight(allocator), [](ILightRenderable* lightToDelete) { 
                            lightToDelete->~ILightRenderable();
                        })
                    );
                }
            }

            // static meshes
            if (auto staticMesh = dynamic_cast<StaticMeshEntity*>(entity))
            {
                meshInstances.push_back(staticMesh->getMeshInstance());
                glm::mat4 model = staticMesh->getWorldTransformMatrix();
                transformSsbo->addElement(model);
            }
        }

        if (!packedGeometry)
            packedGeometry = new PackedGeometry(*inScene);

        std::unordered_map<std::string, u32> materialMap;
        std::unordered_map<std::string, u64> textureMap;

        // build instance descriptors
        for (u32 i = 0; i < meshInstances.size(); ++i) {
            auto mesh = meshInstances[i]->parent;
            auto entry = packedGeometry->submeshMap.find(mesh->name);
            u32 baseSubmesh = 0;
            if (entry != packedGeometry->submeshMap.end()) {
                baseSubmesh = entry->second;
            }
            for (u32 sm = 0; sm < mesh->numSubmeshes(); ++sm) {
                instances->addElement(InstanceDesc{});
                auto& desc = (*instances)[instances->getNumElements() - 1];
                desc.submesh = baseSubmesh + sm;
                desc.transform = i;
                desc.material = 0;

                if (auto pbr = dynamic_cast<PBRMaterial*>(meshInstances[i]->getMaterial(sm))) {
                    auto matlEntry = materialMap.find(pbr->name);
                    // create a material proxy for each unique material instance
                    if (matlEntry == materialMap.end()) {
                        materialMap.insert({ pbr->name, materials->getNumElements() });
                        desc.material = materials->getNumElements();
                        materials->addElement(Material{ });
                        auto& matlProxy = (*materials)[materials->getNumElements() - 1];
                        matlProxy.flags = glm::uvec4(pbr->parameter.getFlags());

                        // albedo
                        matlProxy.kAlbedo = glm::vec4(pbr->parameter.kAlbedo, 1.f);
                        if (auto albedo = pbr->parameter.albedo) {
                            auto entry = textureMap.find(albedo->name);
                            if (entry == textureMap.end()) {
#if BINDLESS_TEXTURE
                                matlProxy.diffuseMapHandle = albedo->glHandle;
                                if (glIsTextureHandleResidentARB(matlProxy.diffuseMapHandle) == GL_FALSE) {
                                    glMakeTextureHandleResidentARB(matlProxy.diffuseMapHandle);
                                }
#endif
                                textureMap.insert({ albedo->name, matlProxy.diffuseMapHandle });
                            }
                            else {
                                matlProxy.diffuseMapHandle = entry->second;
                            }
                        }

                        // normal map
                        if (auto normal = pbr->parameter.normal) {
                            auto entry = textureMap.find(normal->name);
                            if (entry == textureMap.end()) {
#if BINDLESS_TEXTURE
                                matlProxy.normalMapHandle = normal->glHandle;
                                if (glIsTextureHandleResidentARB(matlProxy.normalMapHandle) == GL_FALSE)
                                {
                                    glMakeTextureHandleResidentARB(matlProxy.normalMapHandle);
                                }
#endif
                                textureMap.insert({ normal->name, matlProxy.normalMapHandle });
                            }
                            else {
                                matlProxy.normalMapHandle = entry->second;
                            }
                        }

                        // metallicRoughness map
                        matlProxy.kMetallicRoughness = glm::vec4(pbr->parameter.kMetallic, pbr->parameter.kRoughness, 0.f, 0.f);
                        if (auto metallicRoughness = pbr->parameter.metallicRoughness)
                        {
                            auto entry = textureMap.find(metallicRoughness->name);
                            if (entry == textureMap.end())
                            {
#if BINDLESS_TEXTURE
                                matlProxy.metallicRoughnessMapHandle = metallicRoughness->glHandle;
                                if (glIsTextureHandleResidentARB(matlProxy.metallicRoughnessMapHandle) == GL_FALSE)
                                {
                                    glMakeTextureHandleResidentARB(matlProxy.metallicRoughnessMapHandle);
                                }
#endif
                                textureMap.insert({ metallicRoughness->name, matlProxy.metallicRoughnessMapHandle });
                            }
                            else
                            {
                                matlProxy.metallicRoughnessMapHandle = entry->second;
                            }
                        }
                    }
                    else
                    {
                        desc.material = matlEntry->second;
                    }
                }
            }
        }

        struct InstanceDescSortKey
        {
            inline bool operator() (const InstanceDesc& lhs, const InstanceDesc& rhs)
            {
                return (lhs.submesh < rhs.submesh);
            }
        };
        // todo: this sort maybe too slow at some point 
        // sort the instance buffer to group submesh instances that share the same submesh right next to each other
        std::sort(instances->ssboStruct.dynamicArray.begin(), instances->ssboStruct.dynamicArray.end(), InstanceDescSortKey());
        drawCalls->addElement(0);
        u32 prev = (*instances)[0].submesh;
        for (u32 i = 1; i < instances->getNumElements(); ++i)
        {
            if (instances->ssboStruct.dynamicArray[i].submesh != prev)
            {
                drawCalls->addElement(i);
            }
            prev = (*instances)[i].submesh;
        }
        drawCalls->addElement(instances->getNumElements());

        // build view data
        SET_SSBO_MEMBER((*viewSsbo), view, camera.view);
        SET_SSBO_MEMBER((*viewSsbo), projection, camera.projection);

        // build lighting data
        skybox = inScene->skybox;
        if (inScene->skyLight)
        {
            irradianceProbe = inScene->skyLight->irradianceProbe.get();
            reflectionProbe = inScene->skyLight->reflectionProbe.get();
        }
    }

    /**
    * Submit rendering data to global gpu buffers
    */
    void RenderableScene::submitSceneData(GfxContext* ctx)
    {
        // todo: avoid repetitively submit redundant data
        {

        }

#define VIEW_SSBO_BINDING 0
#define TRANSFORM_SSBO_BINDING 3
#define INSTANCE_DESC_BINDING 41
#define SUBMESH_BUFFER_BINDING 42
#define VERTEX_BUFFER_BINDING 43
#define INDEX_BUFFER_BINDING 44
#define DRAWCALL_BUFFER_BINDING 45
#define MATERIAL_BUFFER_BINDING 46

        packedGeometry->vertexBuffer.bind(VERTEX_BUFFER_BINDING);
        packedGeometry->indexBuffer.bind(INDEX_BUFFER_BINDING);
        packedGeometry->submeshes.bind(SUBMESH_BUFFER_BINDING);

        // bind global ssbo
        SET_SSBO_MEMBER((*viewSsbo), view, camera.view);
        SET_SSBO_MEMBER((*viewSsbo), projection, camera.projection);
        viewSsbo->update();
        viewSsbo->bind(VIEW_SSBO_BINDING);
        transformSsbo->update();
        transformSsbo->bind((u32)SceneSsboBindings::TransformMatrices);

        instances->update();
        instances->bind(INSTANCE_DESC_BINDING);
        materials->update();
        materials->bind(MATERIAL_BUFFER_BINDING);
        drawCalls->update();
        drawCalls->bind(DRAWCALL_BUFFER_BINDING);

        // shared BRDF lookup texture used in split sum approximation for image based lighting
        if (Texture2DRenderable* BRDFLookupTexture = ReflectionProbe::getBRDFLookupTexture())
        {
            ctx->setPersistentTexture(BRDFLookupTexture, (u32)SceneTextureBindings::BRDFLookupTexture);
        }

        // additional light probes
        if (irradianceProbe)
        {
            ctx->setPersistentTexture(irradianceProbe->m_convolvedIrradianceTexture, 3);
        }
        if (reflectionProbe)
        {
            ctx->setPersistentTexture(reflectionProbe->m_convolvedReflectionTexture, 4);
        }
    }
}