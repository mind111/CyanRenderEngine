#pragma once

#include "World.h"
#include "Scene.h"
#include "AssetImporter.h"
#include "CameraEntity.h"
#include "Mesh.h"
#include "StaticMeshEntity.h"
#include "StaticMeshComponent.h"

namespace Cyan
{
    World::World(const char* name) 
        : m_name(name)
    { 
        // todo: create root entity
        assert(m_rootEntity == nullptr);
        m_rootEntity = createEntity(m_name.c_str(), Transform(), nullptr); // root entity's name is the same as the world
        m_scene = std::make_shared<Scene>(this);
    }

    void World::update()
    {
        // update each entity
        // m_rootEntity->translate(glm::vec3(0.f, 0.001f, 0.f));

        // simulate physics

        // finalize all the transforms
        m_rootEntity->getRootSceneComponent()->finalizeAndUpdateTransform();
    }

    void World::import(const char* filename) 
    {
        AssetImporter::importAsync(this, filename);
    }

    void World::load() 
    { 

    }

    void World::onEntityCreated(std::shared_ptr<Entity> e)
    {
        assert(e != nullptr);
        m_entities.push_back(e);
    }

    Entity* World::createEntity(const char* name, /*const Transform& local,*/ const Transform& localToWorld, Entity* parent)
    {
        std::shared_ptr<Entity> outEntity = nullptr;
        World* world = this;
        if (m_rootEntity == nullptr)
        {
            // we are creating the root entity
            assert(name == m_name.c_str());
            outEntity = std::make_shared<Entity>(world, name, Transform(), nullptr);
        }
        else
        {
            auto parentEntity = (parent == nullptr) ? m_rootEntity : parent;
            outEntity = std::make_shared<Entity>(world, name, localToWorld, parentEntity);
        }
        onEntityCreated(outEntity);
        return outEntity.get();
    }

    PerspectiveCameraEntity* World::createPerspectiveCameraEntity(const char* name, const Transform& localToWorld, Entity* parent, const glm::vec3& lookAt, const glm::vec3& worldUp, const glm::vec2& renderResolution, const Camera::ViewMode& viewMode, f32 fov, f32 n, f32 f)
    {
        assert(m_rootEntity != nullptr);
        // add to World
        auto parentEntity = (parent == nullptr) ? m_rootEntity : parent;
        auto e = std::make_shared<PerspectiveCameraEntity>(
            this, name, localToWorld, parentEntity, // Entity
            lookAt, worldUp, renderResolution, viewMode, // Camera
            fov, n, f // PerspectiveCamera
            );

        // add to Scene
        m_scene->addCamera(e->getCameraComponent()->getCamera());
        onEntityCreated(e);
        return e.get();
    }

    StaticMeshEntity* World::createStaticMeshEntity(const char* name, const Transform& localToWorld, Entity* parent, std::shared_ptr<StaticMesh> mesh)
    {
        assert(m_rootEntity != nullptr);
        // add to World
        auto parentEntity = (parent == nullptr) ? m_rootEntity : parent;
        auto e = std::make_shared<StaticMeshEntity>(
            this, name, localToWorld, parentEntity, // Entity
            mesh // StaticMesh
            );
        onEntityCreated(e);
        return e.get();
    }
}
