#include <iostream>
#include <fstream>
#include <queue>

#include <tiny_gltf/json.hpp>
#include <glm/glm.hpp>
#include <stbi/stb_image.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Mesh.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "GraphicsSystem.h"

namespace Cyan
{
    Scene::Scene(const char* inSceneName, f32 cameraAspectRatio)
        : m_name(inSceneName)
    {
        m_rootEntity = createEntity("Root", Transform());
        m_mainCamera = std::unique_ptr<CameraEntity>(createPerspectiveCamera(
            /*name=*/"Camera",
            /*transform=*/Transform {
                glm::vec3(0.f, 3.2f, 8.f)
            },
            /*inLookAt=*/glm::vec3(0., 1., -1.),
            /*inWorldUp=*/glm::vec3(0.f, 1.f, 0.f),
            /*inFov=*/45.f,
            2.f,
            150.f,
            cameraAspectRatio
        ));
    }

    // todo: optimize this ...
    void Scene::update()
    {
        m_mainCamera->update();

        // update transforms of all entities and scene components
        std::queue<Entity*> entities;
        entities.push(m_rootEntity);

        while (!entities.empty())
        {
            auto e = entities.front();
            entities.pop();
            for (auto child : e->childs)
            {
                entities.push(child);
            }

            if (auto parent = e->parent)
            {
                // update transform of this entity's root scene component
                glm::mat4 parentGlobalMatrix = parent->getRootSceneComponent()->getWorldTransformMatrix();
                auto sceneRoot = e->getRootSceneComponent();
                sceneRoot->m_worldTransform.fromMatrix(parentGlobalMatrix * sceneRoot->m_localTransform.toMatrix());

                // update transform of all owning scene components
                std::queue<Component*> components;
                for (auto child : sceneRoot->children)
                {
                    components.push(child);
                }

                while (!components.empty())
                {
                    auto c = components.front();
                    components.pop();
                    for (auto child : c->children)
                    {
                        components.push(child);
                    }
                    if (auto sc = dynamic_cast<SceneComponent*>(c))
                    {
                        auto parent = sc->parent;
                        if (parent)
                        {
                            auto parentSceneComponent = dynamic_cast<SceneComponent*>(parent);
                            while (!parentSceneComponent)
                            {
                                if (parent->parent)
                                {
                                    parent = parent->parent;
                                    parentSceneComponent = dynamic_cast<SceneComponent*>(parent);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            assert(parentSceneComponent);
                            sc->m_worldTransform.fromMatrix(parentSceneComponent->getWorldTransformMatrix() * sc->m_localTransform.toMatrix());
                        }
                    }
                }
            }
        }
    }

    Entity* Scene::createEntity(const char* name, const Transform& transform, Entity* inParent, u32 properties)
    {
        Entity* entity = new Entity(this, name, transform, inParent, properties);
        m_entities.push_back(entity);
        return entity;
    }

    StaticMeshEntity* Scene::createStaticMeshEntity(const char* name, const Transform& transform, Mesh* inMesh, Entity* inParent, u32 properties)
    {
        auto staticMesh = new StaticMeshEntity(this, name, transform, inMesh, inParent, properties);
        m_entities.push_back(staticMesh);
        return staticMesh;
    }

    CameraEntity* Scene::createPerspectiveCamera(const char* name, const Transform& transform, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio, Entity* inParent, u32 properties)
    {
        auto camera = new CameraEntity(this, name, transform, inLookAt, inWorldUp, inFov, inN, inF, inAspectRatio, inParent, properties);
        m_entities.push_back(camera);
        return camera;
    }

    DirectionalLightEntity* Scene::createDirectionalLight(const char* name, const glm::vec3& direction, const glm::vec4& colorAndIntensity)
    {
        DirectionalLightEntity* directionalLight = new DirectionalLightEntity(this, name, Transform(), nullptr, direction, colorAndIntensity, true);
        m_entities.push_back(directionalLight);
        return directionalLight;
    }

    SkyLight* Scene::createSkyLight(const char* name, const glm::vec4& colorAndIntensity)
    {
        if (skyLight)
        {
            cyanError("There is already a skylight exist in scene %s", this->m_name);
            return nullptr;
        }
    }

    SkyLight* Scene::createSkyLightFromSkybox(Skybox* srcSkybox) 
    {
        return nullptr;
    }

    SkyLight* Scene::createSkyLight(const char* name, const char* srcHDRI) 
    { 
        if (skyLight)
        {
            cyanError("There is already a skylight exist in scene %s", this->m_name);
            return nullptr;
        }
        skyLight = new SkyLight(this, glm::vec4(1.f), srcHDRI);
        return skyLight;
    }

    Skybox* Scene::createSkybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution) 
    {
        if (skybox) 
        {
            cyanError("There is already a skybox exist in scene %s", this->m_name);
            return nullptr;
        }
        skybox = new Skybox(name, srcHDRIPath, resolution);
        return skybox;
    }

    void Scene::createPointLight(const char* name, const glm::vec3 position, const glm::vec4& colorAndIntensity)
    {

    }
}
