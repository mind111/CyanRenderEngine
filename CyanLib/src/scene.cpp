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

namespace Cyan
{
    Scene::Scene(World* world)
        : m_world(world)
    {

    }

    void Scene::addCamera(Camera* camera)
    {
        m_cameras.push_back(camera);

        if (camera->bEnabledForRendering)
        {
            m_renders.push_back(std::make_shared<SceneRender>(this, camera));
        }
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
