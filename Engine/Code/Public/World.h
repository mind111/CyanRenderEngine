#pragma once

#include <string>
#include <memory>

#include "Core.h"
#include "Engine.h"
#include "Transform.h"
#include "Entity.h"

namespace Cyan
{
    class Entity;
    class StaticMeshEntity;
    class SceneCamera;

    class ENGINE_API World
    {
    public:
        friend class Engine;

        World(const char* name);
        ~World();

        void update();
        void import(const char* filename);

        template<typename T>
        std::shared_ptr<T> createEntity(const char* name, const Transform& localTransform)
        {
            World* world = this;
            if (m_root == nullptr)
            {
                // we are creating the root entity
                assert(name == m_name.c_str());
                return std::make_shared<T>(world, name, Transform());
            }

            std::shared_ptr<T> outEntity = nullptr;
            outEntity = std::make_shared<T>(world, name, localTransform);
            // by default attach the newly created entity to the root entity first
            m_root->attachChild(outEntity);
            return outEntity;
        }

        template <typename T>
        T* findEntity(const char* name)
        {
            Entity* foundEntity = nullptr;
            std::queue<Entity*> q;
            q.push(m_root.get());

            while (!q.empty())
            {
                auto e = q.front();
                if (e->getName() == name)
                {
                    foundEntity = e;
                    break;
                }

                for (auto& child : e->m_children)
                {
                    q.push(child.get());
                }

                q.pop();
            }

            return dynamic_cast<T*>(foundEntity);
        }

        Scene* getScene() { return m_sceneRenderThread.get(); }
        const std::string& getName() { return m_name; }
        void addSceneCamera(SceneCamera* sceneCamera);
        void removeSceneCamera(SceneCamera* sceneCamera);

    private:
        std::string m_name;

        /**/
        std::vector<SceneCamera*> m_cameras;

        /* these member needs to be accessed from the render thread */
        std::unique_ptr<Scene> m_sceneRenderThread = nullptr;
        std::vector<SceneView*> m_views;

        std::shared_ptr<Entity> m_root = nullptr;
        i32 m_frameCount = 0;
        f32 m_elapsedTime = 0.f;
        f32 m_deltaTime = 0.f;
    };
}// 
