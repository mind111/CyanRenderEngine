#include "StaticMesh.h"
#include "GfxContext.h"

namespace Cyan
{
    StaticMesh::StaticMesh(const char* name, u32 numSubMeshes)
        : Asset(name), m_numSubMeshes(numSubMeshes)
    {
        m_subMeshes.resize(numSubMeshes);
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
            instanceID = m_instances.size();
        }
        return std::move(std::make_unique<StaticMeshInstance>(this, instanceID, localToWorld));
    }

    void StaticMesh::removeInstance(StaticMeshInstance* instance)
    {
        i32 id = instance->getInstanceID();
        assert(id >= 0);
        m_instances[id] = nullptr;
        // recycle the id
        m_freeInstanceIDList.push(id);
    }

#define ENQUEUE_GFX_TASK(task)

    void StaticMesh::setSubMesh(std::unique_ptr<StaticSubMesh> sm, u32 slot)
    {
        assert(slot < m_numSubMeshes);

        ENQUEUE_GFX_TASK([this, std::move(sm), slot]() {
            sm->createGfxProxy();
            m_subMeshes[slot] = std::move(sm);
        })
    }

    StaticSubMesh::StaticSubMesh(StaticMesh* parent, std::unique_ptr<Geometry> geometry)
        : m_parent(parent), m_geometry(std::move(geometry))
    {
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

    StaticMeshInstance::StaticMeshInstance(StaticMesh* parent, i32 instanceID, const Transform& localToWorld)
        : m_parent(parent), m_localToWorldTransform(localToWorld), m_localToWorldMatrix(localToWorld.toMatrix())
    {
    }

    StaticMeshInstance::~StaticMeshInstance()
    {
        m_parent->removeInstance(this);
    }

    void StaticMeshInstance::setLocalToWorldTransform(const Transform& localToWorld)
    {
        m_localToWorldTransform = localToWorld;
        m_localToWorldMatrix = m_localToWorldTransform.toMatrix();
    }
}
