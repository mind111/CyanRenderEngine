#pragma once

#include <string>
#include <memory>

#include "Core.h"
#include "Engine.h"
#include "Transform.h"
#include "Entity.h"

namespace Cyan
{
    class IScene;
    class Entity;
    class StaticMeshEntity;

    class ENGINE_API World
    {
    public:
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

        IScene* getScene() { return m_scene.get(); }
        const std::string& getName() { return m_name; }

    private:
        std::string m_name;
        std::unique_ptr<IScene> m_scene = nullptr;
        std::shared_ptr<Entity> m_root = nullptr;
        i32 m_frameCount = 0;
        f32 m_elapsedTime = 0.f;
        f32 m_deltaTime = 0.f;
    };
}
