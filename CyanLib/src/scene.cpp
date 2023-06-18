#include <iostream>
#include <fstream>
#include <queue>

#include <tiny_gltf/json.hpp>
#include <glm/glm.hpp>
#include <stbi/stb_image.h>
#include <glm/gtc/matrix_transform.hpp>

#include "World.h"
#include "Mesh.h"
#include "CyanAPI.h"
#include "Scene.h"
#include "GraphicsSystem.h"
#include "LightComponents.h"
#include "SceneRender.h"
#include "Skybox.h"

namespace Cyan
{
    Scene::Scene(World* world)
        : m_world(world)
    {

    }

    void Scene::render()
    {
        for (auto sceneCamera : m_sceneCameras)
        {
            sceneCamera->render();
        }
    }

    void Scene::addSceneCamera(SceneCamera* sceneCamera)
    {
        m_sceneCameras.push_back(sceneCamera);
        sceneCamera->setScene(this);
    }

    void Scene::addStaticMeshInstance(StaticMesh::Instance* staticMeshInstance)
    {
        m_staticMeshInstances.push_back(staticMeshInstance);
    }

    void Scene::addDirectionalLight(DirectionalLight* directionalLight)
    {
        if (m_directionalLight != nullptr)
        {
            removeDirectionalLight();
        }
        m_directionalLight = directionalLight;
    }

    void Scene::removeDirectionalLight()
    {
        m_directionalLight = nullptr;
    }

    void Scene::addSkyLight(SkyLight* skyLight)
    {
        if (m_skyLight != nullptr)
        {
            removeSkyLight();
        }
        m_skyLight = skyLight;
    }

    void Scene::removeSkyLight()
    {
        m_skyLight = nullptr;
    }

    void Scene::addSkybox(Skybox* skybox)
    {
        if (m_skybox != nullptr)
        {
            removeSkybox();
        }
        m_skybox = skybox;
    }

    void Scene::removeSkybox()
    {
        m_skybox = nullptr;
    }

    i32 Scene::getFrameCount()
    {
        if (m_world != nullptr)
        {
            return m_world->m_frameCount;
        }
        return -1;
    }

    f32 Scene::getElapsedTime()
    {
        if (m_world != nullptr)
        {
            return m_world->m_elapsedTime;
        }
        return 0.f;
    }

    f32 Scene::getDeltaTime()
    {
        if (m_world != nullptr)
        {
            return m_world->m_deltaTime;
        }
        return 0.f;
    }
}
