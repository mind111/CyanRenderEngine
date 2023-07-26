#include "StaticMesh.h"

namespace Cyan
{
    StaticMesh::StaticMesh(const char* name, u32 numSubMeshes)
        : Asset(name), m_numSubMeshes(numSubMeshes)
    {
        m_subMeshes.resize(numSubMeshes);
        // create all subMeshes here upfront
        for (u32 i = 0; i < m_numSubMeshes; ++i)
        {
            m_subMeshes[i] = std::make_unique<StaticSubMesh>(this);
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

    StaticSubMesh::StaticSubMesh(StaticMesh* parent)
        : m_parent(parent)
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

    void StaticSubMesh::setGeometry(std::unique_ptr<Geometry> geometry)
    {
        m_geometry = std::move(geometry);
        onLoaded();
    }

    StaticMeshInstance::StaticMeshInstance(StaticMesh* parent, i32 instanceID, const std::string& instanceKey, const Transform& localToWorld)
        : m_instanceID(instanceID), m_instanceKey(instanceKey), m_parent(parent), m_localToWorldTransform(localToWorld), m_localToWorldMatrix(localToWorld.toMatrix())
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
