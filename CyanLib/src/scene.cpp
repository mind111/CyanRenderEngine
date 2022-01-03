#include <iostream>
#include <fstream>

#include "json.hpp"
#include "glm.hpp"
#include "stb_image.h"
#include "gtc/matrix_transform.hpp"

#include "CyanAPI.h"
#include "Scene.h"

// ray cast in world space
RayCastInfo Scene::castRay(glm::vec3& ro, glm::vec3& rd, EntityFilter filter, bool debugPrint)
{
    RayCastInfo closestHit;
    for (u32 i = 0; i < entities.size();  ++i)
    {
        auto entity = entities[i];
        bool flag = true;
        switch (filter)
        {
            case EntityFilter::BakeInLightMap:
                flag = entity->m_static;
                break;
            default:
                printf("Unknown entity filter! \n");
        };

        if (flag)
        {
            auto traceInfo = entity->intersectRay(ro, rd, glm::mat4(1.f)); 
            if (traceInfo.t > 0.f && traceInfo < closestHit)
            {
                closestHit = traceInfo;
            }
        }
    }
    if (debugPrint)
        printf("Cast a ray from mouse that hits %s \n", closestHit.m_node->m_name);

    if (closestHit.smIndex < 0 || closestHit.triIndex < 0)
        closestHit.t = -1.f;
    return closestHit;
}

bool Scene::castVisibilityRay(const glm::vec3& ro, glm::vec3& rd, EntityFilter filter)
{
    for (auto entity : entities)
    {
        bool flag = true;
        switch (filter)
        {
            case EntityFilter::BakeInLightMap:
                flag = entity->m_static; 
                break;
            default:
                printf("Unknown entity filter! \n");
        };

        if (flag)
        {
            if (entity->castVisibilityRay(ro, rd, glm::mat4(1.f)))
                return true; 
        }
    }
    return false;
}

void Scene::addStandardPbrMaterial(Cyan::StandardPbrMaterial* matl)
{
    m_materials.push_back(matl);
}

SceneManager* SceneManager::s_sceneManager = 0u;

SceneManager::SceneManager()
{
    if (!s_sceneManager)
    {
        s_sceneManager = this;
    }
    else {
        CYAN_ASSERT(0, "There should be only one instance of SceneManager")
    }

    m_probeFactory = new Cyan::LightProbeFactory();
}

SceneManager* SceneManager::getSingletonPtr()
{
    return s_sceneManager;
}

void SceneManager::setLightProbe(Scene* scene, DistantLightProbe* probe)
{
    scene->m_lastProbe = scene->m_currentProbe;
    scene->m_currentProbe = probe;
}

Entity* SceneManager::createEntity(Scene* scene, const char* entityName, Transform transform, bool isStatic, Entity* parent)
{
    // id
    u32 id = allocEntityId(scene);
    Entity* parentEntity = !parent ? scene->m_rootEntity : parent;
    Entity* newEntity = new Entity(scene, entityName, id, transform, parentEntity, isStatic);
    scene->entities.push_back(newEntity);
    return newEntity; 
}

void SceneManager::traverseScene(Scene* scene)
{
    for (u32 i = 0; i < (u32)scene->entities.size(); ++i)
    {
        std::queue<SceneNode*> nodes;
        nodes.push(scene->entities[i]->m_sceneRoot);
        while(!nodes.empty())
        {
            auto* node = nodes.front();
            nodes.pop();
            for (u32 i = 0; i < node->m_child.size(); ++i)
                nodes.push(node->m_child[i]);
        }
    }
}

void SceneManager::createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity)
{
    CYAN_ASSERT(scene->dLights.size() < Scene::kMaxNumDirLights, "Too many directional lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "DirLight%u", (u32)scene->dLights.size());
    Entity* entity = createEntity(scene, nameBuff, Transform(), true); 
    DirectionalLight light(entity, glm::vec4(color, intensity), glm::vec4(direction, 0.f));
    scene->dLights.push_back(light);
}

void SceneManager::createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity)
{
    CYAN_ASSERT(scene->pLights.size() < Scene::kMaxNumPointLights, "Too many point lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "PointLight%u", (u32)scene->pLights.size());
    Transform transform = Transform();
    transform.m_translate = glm::vec3(position);
    transform.m_scale = glm::vec3(0.1f);
    Entity* entity = createEntity(scene, nameBuff, Transform(), false); 
    Cyan::Mesh* sphereMesh = Cyan::getMesh("sphere_mesh");
    CYAN_ASSERT(sphereMesh, "sphere_mesh does not exist")
    SceneNode* meshNode = Cyan::createSceneNode(scene, "LightMesh", transform, sphereMesh); 
    entity->m_sceneRoot->attach(meshNode);
    Shader* pointLightShader = Cyan::createShader("PointLightShader", "../../shader/shader_light.vs", "../../shader/shader_light.fs");
    Cyan::MaterialInstance* matl = Cyan::createMaterial(pointLightShader)->createInstance();
    meshNode->m_meshInstance->setMaterial(0, matl);
    glm::vec4 u_color = glm::vec4(color, intensity);
    matl->set("color", &u_color.x);

    PointLight light(entity, glm::vec4(color, intensity), glm::vec4(position, 1.f));
    scene->pLights.push_back(light);
}

void SceneManager::updateSceneGraph(Scene* scene)
{

}

// update light data and pack them in a buffer 
void SceneManager::buildLightList(Scene* scene, std::vector<PointLightGpuData>& pLights, std::vector<DirLightGpuData>& dLights)
{
    for (auto& light : scene->dLights)
    {
        light.update();
        dLights.push_back(light.getData());
    }
    for (auto& light : scene->pLights)
    {
        light.update();
        pLights.push_back(light.getData());
    }
}

Cyan::IrradianceProbe* SceneManager::createIrradianceProbe(Scene* scene, const glm::vec3& pos)
{
    auto probe = m_probeFactory->createIrradianceProbe(scene, pos); 
    // TODO: this need to be removed
    scene->m_irradianceProbe = probe;
    return probe;
}

Cyan::ReflectionProbe* SceneManager::createReflectionProbe(Scene* scene, const glm::vec3& pos)
{
    auto probe = m_probeFactory->createReflectionProbe(scene, pos); 
    return probe;
}

Cyan::LightFieldProbe* SceneManager::createLightFieldProbe(Scene* scene, const glm::vec3& pos)
{
    auto probe = m_probeFactory->createLightFieldProbe(scene, pos); 
    return probe;
}

Cyan::LightFieldProbeVolume* SceneManager::createLightFieldProbeVolume(Scene* scene, glm::vec3& pos, glm::vec3& dimension, glm::vec3& spacing)
{
    auto probeVolume = m_probeFactory->createLightFieldProbeVolume(scene, pos, dimension, spacing);
    return probeVolume;
}