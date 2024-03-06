#include "StaticMesh.h"
#include "Material.h"
#include "AssetManager.h"
#include "Engine.h"

#include "Scene.h"
#include "GfxModule.h"
#include "GfxMaterial.h"

namespace Cyan
{
    StaticMesh::StaticMesh(const char* name, u32 numSubMeshes)
        : Asset(name), m_numSubMeshes(numSubMeshes)
    {
        m_subMeshes.resize(numSubMeshes);
        // create all subMeshes here upfront
        for (u32 i = 0; i < m_numSubMeshes; ++i)
        {
            m_subMeshes[i] = std::make_unique<StaticSubMesh>(this, i);
        }
    }

    StaticMesh::~StaticMesh()
    {

    }

    std::unique_ptr<StaticMeshInstance> StaticMesh::createInstance(const Transform& localToWorld)
    {
        i32 instanceID = -1;
        if (!m_freeInstanceIDList.empty())
        {
            instanceID = m_freeInstanceIDList.front();
            m_freeInstanceIDList.pop();
        }
        else
        {
            instanceID = static_cast<i32>(m_instances.size());
        }
        std::string instanceKey = getName() + "_Instance" + std::to_string(instanceID);
        std::unique_ptr<StaticMeshInstance> instance = std::make_unique<StaticMeshInstance>(this, instanceID, instanceKey, localToWorld);
        // setup default material
        auto defaultMaterial = AssetManager::findAsset<Material>("M_DefaultOpaque");
        auto mi = defaultMaterial->getDefaultInstance();
        for (u32 i = 0; i < numSubMeshes(); ++i)
        {
            instance->setMaterial(i, mi);
        }
        m_instances.push_back(instance.get());
        return std::move(instance);
    }

    void StaticMesh::removeInstance(StaticMeshInstance* instance)
    {
        i32 id = instance->getInstanceID();
        assert(id >= 0);
        m_instances[id] = nullptr;
        // recycle the id
        m_freeInstanceIDList.push(id);
    }

    StaticSubMesh::StaticSubMesh(StaticMesh* parent, u32 index)
        : m_parent(parent)
        , m_index(index)
        , m_name()
    {
        m_name = m_parent->getName() + "[" + std::to_string(index) + "]";
    }

    StaticSubMesh::~StaticSubMesh()
    {
    }

    void StaticSubMesh::onLoaded()
    {
        bLoaded.store(true);
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        for (const auto& listener : m_listeners)
        {
            listener(this);
        }
        m_listeners.clear();
        assert(m_listeners.empty() == true);
    }

    void StaticSubMesh::addListener(const Listener& listener)
    {
        bool isLoaded = bLoaded.load();

        // if the subMesh is not ready yet, defer execution
        if (!isLoaded) 
        {
            std::lock_guard<std::mutex> lock(m_listenersMutex);
            m_listeners.push_back(listener);
        }
        else
        {
            // if the subMesh is ready, immediately execute
            listener(this);
        }
    }

    Cyan::GfxStaticSubMesh* StaticSubMesh::getGfxMesh()
    {
        // only allow doing this on the render thread for now
        assert(GfxModule::isInRenderThread());

        const std::string& submeshKey = getName();
        return GfxStaticSubMesh::find(submeshKey);
    }

    void StaticSubMesh::setGeometry(std::unique_ptr<Geometry> geometry)
    {
        m_geometry = std::move(geometry);
        onLoaded();
    }

    StaticMeshInstance::StaticMeshInstance(StaticMesh* parent, i32 instanceID, const std::string& instanceKey, const Transform& localToWorld)
        : m_instanceID(instanceID)
        , m_instanceKey(instanceKey)
        , m_parent(parent)
        , m_localToWorldTransform(localToWorld)
        , m_localToWorldMatrix(localToWorld.toMatrix())
        , m_materials(parent->numSubMeshes())
    {
        // create the default material if it doesn't exist
        auto m = AssetManager::findAsset<Cyan::Material>(Cyan::Material::defaultOpaqueMaterialName);
        if (m == nullptr)
        {
            m = AssetManager::createMaterial(
                Cyan::Material::defaultOpaqueMaterialName,
                Cyan::Material::defaultOpaqueMaterialPath,
                [](Cyan::Material* m) {
                    m->setVec3("mp_albedo", glm::vec3(.95f));
                    m->setFloat("mp_roughness", .5f);
                    m->setFloat("mp_metallic", 0.f);
                    m->setFloat("mp_hasAlbedoMap", 0.f);
                    m->setFloat("mp_hasNormalMap", 0.f);
                    m->setFloat("mp_hasMetallicRoughnessMap", 0.f);
                });
        }
        auto mi = m->getDefaultInstance();
        for (u32 i = 0; i < m_materials.size(); ++i)
        {
            setMaterial(i, mi);
        }
    }

    StaticMeshInstance::~StaticMeshInstance()
    {
        m_parent->removeInstance(this);
    }

    void StaticMeshInstance::addToScene(Scene* scene)
    {
        m_scene = scene;
        StaticMesh& mesh = *getParentMesh();
        for (u32 i = 0; i < mesh.numSubMeshes(); ++i)
        {
            auto mi = getMaterial(i);
            const glm::mat4 localToWorldMatrix = m_localToWorldMatrix;
            std::string instanceKey = std::move(getSubMeshInstanceKey(i));
            mesh[i]->addListener([this, scene, i, mi, localToWorldMatrix, instanceKey](StaticSubMesh* sm) {
                FrameGfxTask task = { };
                task.debugName = std::string("AddStaticMeshInstance");
                Engine::get()->enqueueFrameGfxTask(
                    RenderingStage::kPreSceneRendering,
                    "AddStaticMeshInstance",
                    [i, this, scene, sm, mi, localToWorldMatrix, instanceKey](Frame& frame) {
                        StaticSubMeshInstance instance = { };
                        instance.key = instanceKey;
                        instance.subMesh = GfxStaticSubMesh::create(sm->getName(), sm->getGeometry());
                        instance.material = mi->getGfxMaterialInstance();
                        instance.localToWorldMatrix = localToWorldMatrix;

                        if (scene != nullptr)
                        {
                            scene->addStaticSubMeshInstance(instance);
                        }
                    }
                );

                onAddedToScene();
            });
        }
    }

    void StaticMeshInstance::onAddedToScene()
    {
        Scene* scene = m_scene;
        u32 numSubMeshes = getParentMesh()->numSubMeshes();
        std::vector<std::string> instanceKeys(numSubMeshes);
        for (i32 i = 0; i < instanceKeys.size(); ++i)
        {
            instanceKeys[i] = getSubMeshInstanceKey(i);
        }

        // todo: hmm, is there a way to guarantee that this pointer is valid until the following lambda
        // is executed on the render thread?
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "SyncMaterialChanges",
            [this, scene, numSubMeshes, instanceKeys](Frame& frame) {
                for (u32 i = 0; i < numSubMeshes; ++i)
                {
                    bool bFound;
                    StaticSubMeshInstance& outInstance = scene->findStaticSubMeshInstance(instanceKeys[i], bFound);
                    if (bFound)
                    {
                        /**
                         * For simplicity, instead of capturing materials vector as a copy, just have render thread grab the 
                         * materials vector when necessary. This introduces a lock, but since this vector is not accessed so often
                         * currently, so use of lock here is justified.
                         */
                        auto mi = getMaterial(i);
                        outInstance.material = mi->getGfxMaterialInstance();
                    }
                }
            }
        );
    }

    void StaticMeshInstance::removeFromScene(Scene* scene)
    {

    }

    std::string StaticMeshInstance::getSubMeshInstanceKey(u32 subMeshIndex)
    {
        // subMeshName + instanceID
        StaticMesh& mesh = *getParentMesh();
        const std::string& subMeshName = mesh[subMeshIndex]->getName();
        std::string outKey = subMeshName + "_" + std::to_string(getInstanceID());
        return outKey;
    }

    MaterialInstance* StaticMeshInstance::getMaterial(u32 slot)
    {
        std::lock_guard<std::mutex> lock(m_materialMutex);

        assert(slot < m_materials.size());
        return m_materials[slot];
    }

    void StaticMeshInstance::setLocalToWorldTransform(const Transform& localToWorld)
    {
        m_localToWorldTransform = localToWorld;
        m_localToWorldMatrix = m_localToWorldTransform.toMatrix();

        // copy the state to make sure thread safety
        Scene* scene = m_scene;
        u32 numSubMeshes = getParentMesh()->numSubMeshes();
        glm::mat4 localToWorldMatrix = m_localToWorldMatrix;
        // todo: if numSubMeshes is large, this can get slow, is there any ways to avoid the copy?
        std::vector<std::string> instanceKeys(numSubMeshes);
        for (i32 i = 0; i < instanceKeys.size(); ++i)
        {
            instanceKeys[i] = getSubMeshInstanceKey(i);
        }

        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "UpdateStaticMeshInstanceTransform",
            [scene, numSubMeshes, instanceKeys, localToWorldMatrix](Frame& frame) {
                if (scene != nullptr)
                {
                    for (u32 i = 0; i < numSubMeshes; ++i)
                    {
                        bool bFound;
                        StaticSubMeshInstance& outInstance = scene->findStaticSubMeshInstance(instanceKeys[i], bFound);
                        if (bFound)
                        {
                            outInstance.localToWorldMatrix = localToWorldMatrix;
                        }
                    }
                }
            }
        );
    }

    /**
     * todo: thread safety needs to be improved regarding material pointers, as capturing material pointers doesn't
     * guarantee that it won't be released on the main thread while the render thread is trying to access it. Think about
     * under what cases will the Material pointer be destroyed. Using shared_ptr here makes the copy save, and prevent the main
     * thread from releasing the Material pointer while render thread is still using it. For now, this is now an issue since Material
     * pointer never gets freed, so it's always valid.
     */
    void StaticMeshInstance::setMaterial(i32 slot, MaterialInstance* mi)
    {
        {
            std::lock_guard<std::mutex> lock(m_materialMutex);

            assert(slot < m_materials.size());
            m_materials[slot] = mi;
        }

        Scene* scene = m_scene;
        std::string instanceKey = std::move(getSubMeshInstanceKey(slot));

        // replicate changes to the rendering side data
        Engine::get()->enqueueFrameGfxTask(
            RenderingStage::kPreSceneRendering,
            "setMaterial",
            [scene, mi, slot, instanceKey](Frame& frame) {
                if (scene != nullptr)
                {
                    bool bFound;
                    StaticSubMeshInstance& outInstance = scene->findStaticSubMeshInstance(instanceKey, bFound);
                    if (bFound)
                    {
                        outInstance.material = mi->getGfxMaterialInstance();
                    }
                    else
                    {
                        /**
                         * If scene is not null and the instance is not found, it means that the instance is not added yet due to reasons including
                         * 1. the mesh instance is waiting on mesh to be loaded and then added into the scene, in this case the execution of updating the
                         * material on the render thread side should be further deferred until the instance arrives in the scene. Can do something like a
                         * event driven approach.
                         * like 
                             scene->addStaticMeshInstanceListener(instanceKey, []() {

                                });
                         *
                         */
                    }
                }
            }
        );
    }
}
