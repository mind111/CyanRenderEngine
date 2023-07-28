#include "World.h"
#include "GfxInterface.h"
#include "AssetManager.h"
#include "StaticMesh.h"
#include "Engine.h"

namespace Cyan
{
    World::World(const char* name)
        : m_name(name)
    {
        assert(m_root == nullptr);
        m_root = createEntity<Entity>(m_name.c_str(), Transform());
    }

    World::~World()
    {

    }

    void World::update()
    {
        // update each entity
        std::queue<Entity*> q;
        q.push(m_root.get());
        while (!q.empty())
        {
            auto e = q.front();
            e->update();
            q.pop();

            for (auto child : e->m_children)
            {
                q.push(child.get());
            }
        }

        // simulate physics

        // finalize all the transforms
        m_root->getRootSceneComponent()->finalizeAndUpdateTransform();
    }

    void World::import(const char* filename)
    {
        AssetManager::import(this, filename);
    }

    void World::addStaticMeshInstance(StaticMeshInstance* staticMeshInstance)
    {
        const std::string& instanceKey = staticMeshInstance->getInstanceKey();
        auto entry = m_staticMeshInstanceMap.find(instanceKey);
        if (entry != m_staticMeshInstanceMap.end())
        {
            // repetitively adding the same instance is not allowed and should never happen
            UNEXPECTED_FATAL_ERROR();
        }
        else
        {
            u32 slot = -1;
            if (!m_emptyStaticMeshInstanceSlots.empty())
            {
                u32 emptySlot = m_emptyStaticMeshInstanceSlots.front();
                m_emptyStaticMeshInstanceSlots.pop();
                m_staticMeshInstances[emptySlot] = staticMeshInstance;
                slot = emptySlot;
            }
            else
            {
                m_staticMeshInstances.push_back(staticMeshInstance);
                slot = static_cast<u32>(m_staticMeshInstances.size()) - 1;
            }

            assert(slot >= 0);
            m_staticMeshInstanceMap.insert({ instanceKey, slot });

            Engine::get()->onStaticMeshInstanceAdded(staticMeshInstance);
        }
    }

    void World::removeStaticMeshInstance(StaticMeshInstance* staticMeshInstance)
    {
        UNREACHABLE_ERROR()

        const std::string& instanceKey = staticMeshInstance->getInstanceKey();
        auto entry = m_staticMeshInstanceMap.find(instanceKey);
        if (entry != m_staticMeshInstanceMap.end())
        {
            u32 slot = entry->second;
            m_staticMeshInstances[slot] = nullptr;
            m_emptyStaticMeshInstanceSlots.push(slot);

            Engine::get()->onStaticMeshInstanceRemoved(staticMeshInstance);
        }
        else
        {
            // disallow removing non-existing instance
            UNEXPECTED_FATAL_ERROR();
        }
    }

    void World::addSceneCamera(SceneCamera* sceneCamera)
    {
        m_cameras.push_back(sceneCamera);

        Engine::get()->onSceneCameraAdded(sceneCamera);
    }

    void World::removeSceneCamera(SceneCamera* sceneCamera)
    {
        bool bFound = false;
        // doing a simple linear search for now since there shouldn't be that many viewing cameras anyway
        for (i32 i = 0; i < m_cameras.size(); ++i)
        {
            if (m_cameras[i] == sceneCamera)
            {
                bFound = true;
                m_cameras.erase(m_cameras.begin() + i);
                break;
            }
        }
        assert(bFound);

        Engine::get()->onSceneCameraRemoved(sceneCamera);
    }
}