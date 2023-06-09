#pragma once

#include "World.h"
#include "Scene.h"
#include "AssetImporter.h"
#include "CameraEntity.h"
#include "Mesh.h"
#include "StaticMeshEntity.h"
#include "StaticMeshComponent.h"
#include "LightEntities.h"
#include "LightComponents.h"
#include "SkyboxEntity.h"

namespace Cyan
{
    World::World(const char* name) 
        : m_name(name)
    { 
        // todo: create root entity
        assert(m_rootEntity == nullptr);
        m_rootEntity = createEntity(m_name.c_str(), Transform()); // root entity's name is the same as the world
        m_scene = std::make_shared<Scene>(this);
    }

    void World::update()
    {
        // update each entity
        std::queue<Entity*> q;
        q.push(m_rootEntity);
        while (!q.empty())
        {
            auto e = q.front();
            e->update();
            q.pop();

            for (auto child : e->m_children)
            {
                q.push(child);
            }
        }

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
        if (m_rootEntity != nullptr)
        {
            m_rootEntity->attachChild(e.get());
        }
        else
        {
            assert(e->isRootEntity());
        }
    }

    Entity* World::createEntity(const char* name, const Transform& local)
    {
        std::shared_ptr<Entity> outEntity = nullptr;
        World* world = this;
        if (m_rootEntity == nullptr)
        {
            // we are creating the root entity
            assert(name == m_name.c_str());
            outEntity = std::make_shared<Entity>(world, name, Transform());
        }
        else
        {
            outEntity = std::make_shared<Entity>(world, name, local);
        }
        onEntityCreated(outEntity);
        return outEntity.get();
    }

    PerspectiveCameraEntity* World::createPerspectiveCameraEntity(const char* name, const Transform& local, const glm::vec3& worldUp, const glm::vec2& renderResolution, const Camera::ViewMode& viewMode, f32 fov, f32 n, f32 f)
    {
        assert(m_rootEntity != nullptr);

        // add to World
        auto e = std::make_shared<PerspectiveCameraEntity>(
            this, name, local, // Entity
            worldUp, renderResolution, viewMode, // Camera
            fov, n, f // PerspectiveCamera
            );

        // add to Scene
        m_scene->addCamera(e->getCameraComponent()->getCamera());

        onEntityCreated(e);
        return e.get();
    }

    StaticMeshEntity* World::createStaticMeshEntity(const char* name, const Transform& local, std::shared_ptr<StaticMesh> mesh)
    {
        assert(m_rootEntity != nullptr);

        // add to World
        auto e = std::make_shared<StaticMeshEntity>(
            this, name, local, // Entity
            mesh // StaticMesh
            );

        onEntityCreated(e);
        return e.get();
    }

    DirectionalLightEntity* World::createDirectionalLightEntity(const char* name, const Transform& local)
    {
        assert(m_rootEntity != nullptr);
        auto e = std::make_shared<DirectionalLightEntity>(this, name, local);
        m_scene->addDirectionalLight(e->getDirectionalLightComponent()->getDirectionalLight());
        onEntityCreated(e);
        return e.get();
    }

    SkyLightEntity* World::createSkyLightEntity(const char* name, const Transform& local)
    {
        assert(m_rootEntity != nullptr);
        auto e = std::make_shared<SkyLightEntity>(this, name, local);
        onEntityCreated(e);
        return e.get();
    }

    SkyboxEntity* World::createSkyboxEntity(const char* name, const Transform& local)
    {
        assert(m_rootEntity != nullptr);
        auto e = std::make_shared<SkyboxEntity>(this, name, local);
        onEntityCreated(e);
        return e.get();
    }
}
