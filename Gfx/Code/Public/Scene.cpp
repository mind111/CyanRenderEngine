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
            slot = static_cast<u32>(m_staticSubMeshInstances.size());
            m_staticSubMeshInstances.push_back(instance);
        }
        m_staticSubMeshInstanceMap.insert({ instance.key, slot });
    }

    void Scene::removeStaticSubMeshInstance()
    {

    }

    void Scene::addDirectionalLight(DirectionalLight* directionalLight)
    {
        assert(m_directionalLight == nullptr);
        m_directionalLight = directionalLight;
    }

    void Scene::removeDirectionalLight(DirectionalLight* directionalLight)
    {
        assert(m_directionalLight != nullptr);
    }

    void Scene::setSkybox(Skybox* skybox)
    {
        // todo:
        assert(m_skybox == nullptr);
        m_skybox = skybox;
    }

    void Scene::unsetSkybox()
    {
        m_skybox = nullptr;
    }

    void Scene::setSkyLight(SkyLight* skylight)
    {
        // todo:
        assert(m_skyLight == nullptr);
        m_skyLight = skylight;
    }

    void Scene::unsetSkyLight()
    {
        m_skyLight = nullptr;
    }
}

