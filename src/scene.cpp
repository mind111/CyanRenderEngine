#include <iostream>
#include <fstream>

#include "json.hpp"
#include "glm.hpp"
#include "stb_image.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "gtc/matrix_transform.hpp"

#include "CyanAPI.h"
#include "Scene.h"

SceneManager sceneManager;


void SceneManager::setLightProbe(Scene* scene, LightProbe* probe)
{
    scene->m_lastProbe = scene->m_currentProbe;
    scene->m_currentProbe = probe;
}

Entity* SceneManager::createEntity(Scene* scene, const char* entityName, Transform transform, Entity* parent)
{
    /* Note(Min): 
        this custom allocator does not work for classes that has virtual methods, need to use placment
        new. Study about why sizeof() won't include size for the vtable or virtual methods?
    */
    // Entity* entity = (Entity*)CYAN_ALLOC(sizeof(Entity));
    // TODO: research about how to use custom allocator with virtual classes
    Entity* entity = new Entity;
    // mesh instance
    // id
    entity->m_entityId = scene->entities.size() > 0 ? scene->entities.size() : 0;
    if (entityName) 
    {
        CYAN_ASSERT(strlen(entityName) < kEntityNameMaxLen, "Entity name too long !!")
        strcpy(entity->m_name, entityName);
    } else {
        char buff[64];
        sprintf(buff, "Entity%u", entity->m_entityId);
    }
    entity->m_sceneRoot = Cyan::createSceneNode("DefaultSceneRoot", transform);
    // transform
    if (parent) 
    {
        parent->attach(entity);
    }
    else
    {
        if (scene->m_rootEntity)
        {
            scene->m_rootEntity->attach(entity);
        }
    }
    scene->entities.push_back(entity);
    return new Entity(entityName, id, transform, parent);
    return entity; 
}

void SceneManager::createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity)
{
    CYAN_ASSERT(scene->dLights.size() < Scene::kMaxNumDirLights, "Too many directional lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "DirLight%u", (u32)scene->dLights.size());
    Entity* entity = createEntity(scene, nameBuff, Transform()); 
    DirectionalLight light(entity, glm::vec4(color, intensity), glm::vec4(direction, 0.f));
    scene->dLights.push_back(light);
}

void SceneManager::createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity)
{
    CYAN_ASSERT(scene->pLights.size() < Scene::kMaxNumPointLights, "Too many point lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "PointLight%u", (u32)scene->pLights.size());
    // TODO: not sure why if I 
    Transform transform = Transform();
    transform.m_translate = glm::vec3(position);
    transform.m_scale = glm::vec3(0.1f);
    Entity* entity = createEntity(scene, nameBuff, Transform()); 
    Cyan::Mesh* sphereMesh = Cyan::getMesh("sphere_mesh");
    CYAN_ASSERT(sphereMesh, "sphere_mesh does not exist")
    SceneNode* meshNode = Cyan::createSceneNode("LightMesh", transform, sphereMesh); 
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
void SceneManager::buildLightList(Scene* scene, std::vector<PointLightData>& pLights, std::vector<DirLightData>& dLights)
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