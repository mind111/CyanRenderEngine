#include "Scene.h"
#include "GfxContext.h"
#include "GfxMaterial.h"

namespace Cyan
{
    Scene::Scene()
    {

    }

    Scene::~Scene()
    {

    }

    StaticSubMeshInstance& Scene::findStaticSubMeshInstance(const std::string& instanceKey, bool& bFound)
    {
        static StaticSubMeshInstance s_defaultInstance { };
        bFound = false;
        auto entry = m_staticSubMeshInstanceMap.find(instanceKey);
        if (entry != m_staticSubMeshInstanceMap.end())
        {
            bFound = true;
            u32 slot = entry->second;
            return m_staticSubMeshInstances[slot];
        }
        return s_defaultInstance;
    }

    void Scene::addStaticSubMeshInstance(const StaticSubMeshInstance& instance)
    {
        auto entry = m_staticSubMeshInstanceMap.find(instance.key);
        if (entry != m_staticSubMeshInstanceMap.end())
        {
            UNREACHABLE_ERROR()
        }
        u32 slot = -1;
        if (!m_freeStaticSubMeshInstanceSlots.empty())
        {
            slot = m_freeStaticSubMeshInstanceSlots.front();
            m_freeStaticSubMeshInstanceSlots.pop();
        }
        else
        {
            slot = m_staticSubMeshInstances.size();
            m_staticSubMeshInstances.push_back(instance);
        }
        m_staticSubMeshInstanceMap.insert({ instance.key, slot });
    }

    void Scene::removeStaticSubMeshInstance()
    {

    }
}

