#include <iostream>
#include <fstream>
#include <queue>

#include "json.hpp"
#include "glm.hpp"
#include "stb_image.h"
#include "gtc/matrix_transform.hpp"

#include "CyanAPI.h"
#include "Scene.h"
#include "GraphicsSystem.h"

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

BoundingBox3f Scene::getBoundingBox()
{
    BoundingBox3f aabb = { };
    for (u32 i = 0; i < entities.size(); ++i)
    {
        std::queue<SceneNode*> nodes;

        auto sceneRoot = entities[i]->m_sceneRoot;
        nodes.push(sceneRoot);

        while (!nodes.empty())
        {
            auto node = nodes.front();
            nodes.pop();
            if (node->m_meshInstance)
                aabb.bound(node->m_meshInstance->getAABB());
            for (u32 i = 0; i < node->m_child.size(); ++i)
                nodes.push(node->m_child[i]);
        }
    }
    return aabb;
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
}

SceneManager* SceneManager::getSingletonPtr()
{
    return s_sceneManager;
}

Entity* SceneManager::createEntity(Scene* scene, const char* entityName, Transform transform, bool isStatic, Entity* parent)
{
    // id
    u32 id = allocEntityId(scene);
    Entity* parentEntity = parent;
    if (id != 0 && !parent)
    {
        parentEntity = scene->m_rootEntity;
    }
    Entity* newEntity = new Entity(scene, entityName, id, transform, parentEntity, isStatic);
    scene->entities.push_back(newEntity);
    return newEntity; 
}

u32 SceneManager::allocSceneNode(Scene* scene)
{
    CYAN_ASSERT(scene->m_numSceneNodes < Scene::kMaxNumSceneNodes, "Too many scene nodes created!");
    return (scene->m_numSceneNodes++);
}

Scene* SceneManager::createScene(const char* name, const char* file)
{
    Cyan::Toolkit::GpuTimer loadSceneTimer("createScene()", true);
    Scene* scene = new Scene;
    scene->m_name = std::string(name);
    scene->g_sceneNodes.resize(Scene::kMaxNumSceneNodes);
    scene->g_localTransforms.resize(Scene::kMaxNumSceneNodes);
    scene->g_globalTransforms.resize(Scene::kMaxNumSceneNodes);
    scene->g_localTransformMatrices.resize(Scene::kMaxNumSceneNodes);
    scene->g_globalTransformMatrices.resize(Scene::kMaxNumSceneNodes);
    scene->m_numSceneNodes = 0;

    scene->m_rootEntity = nullptr;
    // create root entity
    scene->m_rootEntity = SceneManager::getSingletonPtr()->createEntity(scene, "Root", Transform(), true);
    scene->g_sceneRoot = scene->m_rootEntity->m_sceneRoot;
    auto assetManager = Cyan::GraphicsSystem::getSingletonPtr()->getAssetManager(); 
    assetManager->loadScene(scene, file);
    loadSceneTimer.end();
    return scene;
}

SceneNode* SceneManager::createSceneNode(Scene* scene, const char* name, Transform transform, Cyan::Mesh* mesh, bool hasAABB)
{
    u32 handle              = allocSceneNode(scene);
    SceneNode& newNode      = scene->g_sceneNodes[handle];
    newNode.m_scene = scene;
    CYAN_ASSERT(strlen(name) < 128, "SceneNode name %s is too long!", name);
    strcpy(newNode.m_name, name);
    newNode.localTransform  = handle;
    newNode.globalTransform = handle;
    newNode.m_hasAABB = hasAABB;
    newNode.needUpdate = false;
    scene->g_localTransforms[handle]  = transform;
    scene->g_globalTransforms[handle] = Transform();
    scene->g_localTransformMatrices[handle]  = transform.toMatrix();
    scene->g_globalTransformMatrices[handle] = glm::mat4(1.f);
    if (mesh)
    {
        newNode.m_meshInstance = mesh->createInstance(scene);
        if (mesh->m_shouldNormalize)
        {
            glm::mat4 localTransformMat = newNode.m_localTransform.toMatrix() * mesh->m_normalization;
            newNode.setLocalTransform(localTransformMat);
            scene->g_localTransformMatrices[newNode.localTransform] *= mesh->m_normalization;
        }
    }
    return &newNode;
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
    SceneNode* meshNode = SceneManager::getSingletonPtr()->createSceneNode(scene, "LightMesh", transform, sphereMesh); 
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
    std::queue<SceneNode*> nodes;
    nodes.push(scene->g_sceneRoot);
    while (!nodes.empty())
    {
        auto node = nodes.front();
        nodes.pop();
        if (node->m_parent)
        {
            scene->g_globalTransformMatrices[node->globalTransform] = scene->g_globalTransformMatrices[node->m_parent->globalTransform] * scene->g_localTransformMatrices[node->localTransform];
            scene->g_globalTransforms[node->globalTransform].fromMatrix(scene->g_globalTransformMatrices[node->globalTransform]);
        }
        for (u32 i = 0; i < node->m_child.size(); ++i)
        {
            nodes.push(node->m_child[i]);
        }
        for (u32 i = 0; i < node->m_indirectChild.size(); ++i)
        {
            nodes.push(node->m_indirectChild[i]);
        }
    }
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

Cyan::IrradianceProbe* SceneManager::createIrradianceProbe(Cyan::Texture* srcCubemapTexture, const glm::uvec2& irradianceRes)
{
    return new Cyan::IrradianceProbe(srcCubemapTexture, irradianceRes);
}

Cyan::IrradianceProbe* SceneManager::createIrradianceProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceResolution)
{
    return new Cyan::IrradianceProbe(scene, pos, sceneCaptureRes, irradianceResolution);
}

Cyan::ReflectionProbe* createReflectionProbe(Cyan::Texture* srcCubemapTexture)
{
    return new Cyan::ReflectionProbe(srcCubemapTexture);
}

Cyan::ReflectionProbe* SceneManager::createReflectionProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes)
{
    return new Cyan::ReflectionProbe(scene, pos, sceneCaptureRes);
}
