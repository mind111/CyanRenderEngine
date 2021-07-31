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

void SceneNode::addChild(SceneNode* child)
{
    child->m_parent = this;
    m_child.push_back(child);
    child->update();
}

// basic depth first traversal
void SceneNode::update()
{
    if (m_parent)
    {
        m_entity->m_worldTransformMatrix = m_parent->m_entity->m_worldTransformMatrix * m_entity->m_instanceTransform.toMatrix();
    } else {
        m_entity->m_worldTransformMatrix = m_entity->m_instanceTransform.toMatrix();
    }
    for (auto* child : m_child) {
        child->update();
    }
}

void SceneManager::setLightProbe(Scene* scene, LightProbe* probe)
{
    scene->m_lastProbe = scene->m_currentProbe;
    scene->m_currentProbe = probe;
}

SceneNode* SceneManager::createSceneNode(Scene* scene, SceneNode* parent, Entity* entity) {
    // insert into the scene graph, by default all the newly created entity that has a transform
    // component should be child of the root node
    SceneNode* node = Cyan::allocSceneNode();
    node->m_entity = entity;
    glm::mat4 testMat = Transform().toMatrix();
    // root node
    if (!scene->m_root)
    {
        scene->m_root = node;
        CYAN_ASSERT(entity->m_hasTransform, "Root node has to have Transform component")
        scene->m_root->m_entity->m_worldTransformMatrix = scene->m_root->m_entity->m_instanceTransform.toMatrix();
        return scene->m_root;
    }
    if (!entity->m_hasTransform) {
        entity->m_instanceTransform = Transform();
    }
    // non-root node
    if (parent) {
        parent->addChild(node);
    } else {
        scene->m_root->addChild(node);
    }
    return node;
}

Entity* SceneManager::createEntity(Scene* scene, const char* entityName, const char* meshName, Transform transform, bool hasTransform)
{
    Entity* entity = (Entity*)CYAN_ALLOC(sizeof(Entity));
    // mesh instance
    Cyan::Mesh* mesh = Cyan::getMesh(meshName);
    entity->m_meshInstance = mesh ? mesh->createInstance() : nullptr; 
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
    // transform
    entity->m_instanceTransform = transform;
    entity->m_hasTransform = hasTransform;
    scene->entities.push_back(entity);
    return entity; 
}

void SceneManager::createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity)
{
    CYAN_ASSERT(scene->dLights.size() < Scene::kMaxNumDirLights, "Too many directional lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "DirLight%u", (u32)scene->dLights.size());
    Entity* entity = createEntity(scene, nameBuff, nullptr, Transform(), true); 
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
    Entity* entity = createEntity(scene, nameBuff, "sphere_mesh", transform, true); 
    createSceneNode(scene, nullptr, entity); 
    Shader* pointLightShader = Cyan::createShader("PointLightShader", "../../shader/shader_light.vs", "../../shader/shader_light.fs");
    Cyan::MaterialInstance* matl = Cyan::createMaterial(pointLightShader)->createInstance();
    entity->m_meshInstance->setMaterial(0, matl);
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
    scene->m_root->update();
}

void SceneManager::updateDirLights(Scene* scene)
{
    for (auto& light : scene->dLights)
    {
        light.direction = light.baseLight.m_entity->m_worldTransformMatrix * light.direction;
    }
}

void SceneManager::updatePointLights(Scene* scene)
{
    for (auto& light : scene->pLights)
    {
         light.position = light.baseLight.m_entity->m_worldTransformMatrix[3];
    }
}