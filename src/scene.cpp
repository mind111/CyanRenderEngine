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

// SceneNode* SceneManager::createSceneNode(Scene* scene, SceneNode* parent, Entity* entity) {
//     // insert into the scene graph, by default all the newly created entity that has a transform
//     // component should be child of the root node
//     SceneNode* node = Cyan::allocSceneNode();
//     node->m_entity = entity;
//     glm::mat4 testMat = Transform().toMatrix();
//     // root node
//     if (!scene->m_root)
//     {
//         scene->m_root = node;
//         CYAN_ASSERT(entity->m_hasTransform, "Root node has to have Transform component")
//         scene->m_root->m_entity->m_worldTransformMatrix = scene->m_root->m_entity->m_instanceTransform.toMatrix();
//         return scene->m_root;
//     }
//     if (!entity->m_hasTransform) {
//         entity->m_instanceTransform = Transform();
//     }
//     // non-root node
//     if (parent) {
//         parent->attachChild(node);
//     } else {
//         scene->m_root->addChild(node);
//     }
//     return node;
// }

// Entity* SceneManager::createEntity(Scene* scene, const char* entityName, const char* meshName, Transform transform, bool hasTransform)
// {
//     Entity* entity = (Entity*)CYAN_ALLOC(sizeof(Entity));
//     // mesh instance
//     Cyan::Mesh* mesh = Cyan::getMesh(meshName);
//     entity->m_meshInstance = mesh ? mesh->createInstance() : nullptr; 
//     // id
//     entity->m_entityId = scene->entities.size() > 0 ? scene->entities.size() : 0;
//     if (entityName) 
//     {
//         CYAN_ASSERT(strlen(entityName) < kEntityNameMaxLen, "Entity name too long !!")
//         strcpy(entity->m_name, entityName);
//     } else {
//         char buff[64];
//         sprintf(buff, "Entity%u", entity->m_entityId);
//     }
//     // transform
//     entity->m_instanceTransform = transform;
//     entity->m_hasTransform = hasTransform;
//     scene->entities.push_back(entity);
//     return entity; 
// }

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
    return entity; 
}

void SceneManager::createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity)
{
    CYAN_ASSERT(scene->dLights.size() < Scene::kMaxNumDirLights, "Too many directional lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "DirLight%u", (u32)scene->dLights.size());
    Entity* entity = createEntity(scene, nameBuff, Transform()); 
    DirectionalLight light = {
        {
            entity,
            {0.f, 0.f},
            glm::vec4(color, intensity)
        },
        glm::vec4(direction, 0.f)
    };
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
    Entity* entity = createEntity(scene, nameBuff, transform); 
    Cyan::Mesh* sphereMesh = Cyan::getMesh("sphere_mesh");
    CYAN_ASSERT(sphereMesh, "sphere_mesh does not exist")
    SceneNode* meshNode = Cyan::createSceneNode("sphere_mesh", Transform(), sphereMesh); 
    entity->m_sceneRoot->attach(meshNode);
    Shader* pointLightShader = Cyan::createShader("PointLightShader", "../../shader/shader_light.vs", "../../shader/shader_light.fs");
    Cyan::MaterialInstance* matl = Cyan::createMaterial(pointLightShader)->createInstance();
    meshNode->m_meshInstance->setMaterial(0, matl);
    glm::vec4 u_color = glm::vec4(color, intensity);
    matl->set("color", &u_color.x);

    PointLight light = {
        {
            entity,
            {0.f, 0.f},
            glm::vec4(color, intensity)
        },
        glm::vec4(position, 1.f)
    };
    scene->pLights.push_back(light);
}

void SceneManager::updateSceneGraph(Scene* scene)
{

}

void SceneManager::updateDirLights(Scene* scene)
{
    for (auto& light : scene->dLights)
    {
        light.direction = light.baseLight.m_entity->getWorldTransform().toMatrix() * light.direction;
    }
}

void SceneManager::updatePointLights(Scene* scene)
{
    for (auto& light : scene->pLights)
    {
         light.position = glm::vec4(light.baseLight.m_entity->getWorldTransform().m_translate, 1.0);
    }
}