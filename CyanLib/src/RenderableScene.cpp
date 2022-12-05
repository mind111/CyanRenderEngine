#include <unordered_map>

#include "RenderableScene.h"
#include "Camera.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "LightComponent.h"
#include "LightRenderable.h"

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
                    materialMap.insert({ pbr->name, materials->getNumElements() });
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
                    materials->addElement(matlProxy);
                    return materials->getNumElements() - 1;
                }
                else
                {
                    return matlEntry->second;
                }
            }
        }
    }

    SceneRenderable::SceneRenderable(const Scene* inScene, const SceneView& sceneView, LinearAllocator& allocator)
    {
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

        viewSsbo = std::make_unique<ViewBuffer>();
        transformSsbo = std::make_unique<TransformBuffer>();
        instances = std::make_unique<InstanceBuffer>();
        drawCalls = std::make_unique<DrawCallBuffer>();
        materials = std::make_unique<MaterialBuffer>();

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

        // std::unordered_map<std::string, u32> materialMap;
        // std::unordered_map<std::string, u64> textureMap;

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
                // auto& desc = (*instances)[instances->getNumElements() - 1];
                desc.submesh = baseSubmesh + sm;
                desc.transform = i;
#if 1
                desc.material = getMaterialID(meshInstances[i], sm);
#else
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
#endif
                instances->addElement(desc);
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
        std::sort(instances->data.array.begin(), instances->data.array.end(), InstanceDescSortKey());
        drawCalls->addElement(0);
        u32 prev = (*instances)[0].submesh;
        for (u32 i = 1; i < instances->getNumElements(); ++i)
        {
            if (instances->data.array[i].submesh != prev)
            {
                drawCalls->addElement(i);
            }
            prev = (*instances)[i].submesh;
        }
        drawCalls->addElement(instances->getNumElements());

        // build view data
        viewSsbo->data.constants.view = camera.view;
        viewSsbo->data.constants.projection = camera.projection;

        // build lighting data
        skybox = inScene->skybox;
        if (inScene->skyLight)
        {
            irradianceProbe = inScene->skyLight->irradianceProbe.get();
            reflectionProbe = inScene->skyLight->reflectionProbe.get();
        }
    }

    void SceneRenderable::clone(SceneRenderable& dst, const SceneRenderable& src) {
        dst.camera = src.camera;
        dst.meshInstances = src.meshInstances;
        dst.skybox = src.skybox;
        for (auto light : src.lights) {
            // todo: as of right now copying lights will be shallow, need to 
            // actually implement light->clone() later and convert this to a deep copy
            // dst.lights.push_back(std::shared_ptr<ILightRenderable>(light->clone()));
            dst.lights.push_back(light);
        }
        dst.irradianceProbe = src.irradianceProbe;
        dst.reflectionProbe = src.reflectionProbe;
        dst.viewSsbo = std::unique_ptr<ViewBuffer>(src.viewSsbo->clone());
        dst.transformSsbo = std::unique_ptr<TransformBuffer>(src.transformSsbo->clone());
        dst.instances = std::unique_ptr<InstanceBuffer>(src.instances->clone());
        dst.drawCalls = std::unique_ptr<DrawCallBuffer>(src.drawCalls->clone());
        dst.materials = std::unique_ptr<MaterialBuffer>(src.materials->clone());
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
    void SceneRenderable::submitSceneData(GfxContext* ctx)
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

        viewSsbo->data.constants.view = camera.view;
        viewSsbo->data.constants.projection = camera.projection;
        viewSsbo->upload();
        viewSsbo->bind(VIEW_SSBO_BINDING);
        transformSsbo->upload();
        transformSsbo->bind((u32)SceneSsboBindings::TransformMatrices);

        instances->upload();
        instances->bind(INSTANCE_DESC_BINDING);
        materials->upload();
        materials->bind(MATERIAL_BUFFER_BINDING);
        drawCalls->upload();
        drawCalls->bind(DRAWCALL_BUFFER_BINDING);
    }
}