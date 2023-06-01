#pragma once

#include <memory>

#include "Common.h"
#include "Camera.h"
#include "Transform.h"

namespace Cyan
{
    class Scene;
    class Entity;
    class PerspectiveCameraEntity;
    class StaticMesh;
    class StaticMeshEntity;
    class Camera;

    // entity ownership?
    class World
    {
    public:
        World(const char* inName);
        ~World() { }

        void update();
        void import(const char* filename);
        void load();

        Scene* getScene() { return m_scene.get(); }

        void onEntityCreated(std::shared_ptr<Entity> entity);
        Entity* createEntity(const char* name, /*const Transform& local,*/ const Transform& localToWorld, Entity* parent);
        PerspectiveCameraEntity* createPerspectiveCameraEntity(const char* name, const Transform& localToWorld, Entity* parent, const glm::vec3& lookAt, const glm::vec3& worldUp, const glm::vec2& renderResolution, const Camera::ViewMode& viewMode, f32 fov, f32 n, f32 f);
        StaticMeshEntity* createStaticMeshEntity(const char* name, const Transform& localToWorld, Entity* parent, std::shared_ptr<StaticMesh> mesh);

        std::string m_name;
        std::shared_ptr<Scene> m_scene = nullptr;
        Entity* m_rootEntity = nullptr;
        std::vector<std::shared_ptr<Entity>> m_entities;
        bool bIsPaused = false;
        i32 m_frameCount = 0;
        f32 m_elapsedTime = 0.f;
        f32 m_deltaTime = 0.f;
    };
}