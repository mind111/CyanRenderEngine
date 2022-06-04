#include <iostream>
#include <fstream>
#include <queue>

#include "json.hpp"
#include "glm.hpp"
#include "stb_image.h"
#include "gtc/matrix_transform.hpp"

#include "Mesh.h"
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
            case EntityFilter::kAll:
                break;
            case EntityFilter::BakeInLightMap:
                flag = entity->m_static;
                break;
            default:
                printf("Unknown entity filter! \n");
        };

        if (flag)
        {
#if 0
            auto traceInfo = entity->intersectRay(ro, rd, glm::mat4(1.f)); 
            if (traceInfo.t > 0.f && traceInfo < closestHit)
            {
                closestHit = traceInfo;
            }
#endif
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
#if 0
            if (entity->castVisibilityRay(ro, rd, glm::mat4(1.f)))
                return true; 
#endif
        }
    }
    return false;
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

SceneManager* SceneManager::get()
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
    CYAN_ASSERT(scene->m_numSceneNodes < Scene::kMaxNumSceneComponents, "Too many scene nodes created!");
    return (scene->m_numSceneNodes++);
}

std::shared_ptr<Scene> SceneManager::importScene(const char* name, const char* file)
{
    Cyan::Toolkit::GpuTimer loadSceneTimer("importScene()", true);
    std::shared_ptr<Scene> scene = std::make_shared<Scene>();
    scene->m_name = std::string(name);

    scene->m_rootEntity = nullptr;
    scene->m_rootEntity = SceneManager::get()->createEntity(scene.get(), "Root", Transform(), true);
    scene->g_sceneRoot = scene->m_rootEntity->m_sceneRoot;
    scene->aabb.init();
    auto assetManager = Cyan::GraphicsSystem::get()->getAssetManager(); 
    assetManager->importScene(scene.get(), file);
    loadSceneTimer.end();
    return scene;
}

SceneComponent* SceneManager::createSceneNode(Scene* scene, const char* name, Transform transform)
{
    SceneComponent* sceneNode = scene->sceneComponentPool.alloc();
    
    Transform* localTransform = scene->localTransformPool.alloc();
    *localTransform = transform;
    auto localTransformMatrix = scene->localTransformMatrixPool.alloc();
    *localTransformMatrix = transform.toMatrix();
    sceneNode->localTransform = scene->localTransformPool.getObjectIndex(localTransform);

    Transform* globalTransfom = scene->globalTransformPool.alloc();
    *globalTransfom = Transform();
    auto globalTransformMatrix = scene->globalTransformMatrixPool.alloc();
    *globalTransformMatrix = glm::mat4(1.f);
    sceneNode->globalTransform = scene->globalTransformPool.getObjectIndex(globalTransfom);

    sceneNode->m_scene = scene;
    CYAN_ASSERT(strlen(name) < 128, "SceneNode name %s is too long!", name);
    strcpy(sceneNode->m_name, name);
    scene->sceneNodes.push_back(sceneNode);
    return sceneNode;
}

MeshComponent* SceneManager::createMeshNode(Scene* scene, Transform transform, Cyan::Mesh* mesh)
{
    MeshComponent* meshNode = scene->meshComponentPool.alloc();
    
    Transform* localTransform = scene->localTransformPool.alloc();
    *localTransform = transform;
    auto localTransformMatrix = scene->localTransformMatrixPool.alloc();
    *localTransformMatrix = transform.toMatrix();
    meshNode->localTransform = scene->localTransformPool.getObjectIndex(localTransform);

    Transform* globalTransfom = scene->globalTransformPool.alloc();
    *globalTransfom = Transform();
    auto globalTransformMatrix = scene->globalTransformMatrixPool.alloc();
    *globalTransformMatrix = glm::mat4(1.f);
    meshNode->globalTransform = scene->globalTransformPool.getObjectIndex(globalTransfom);

    meshNode->m_scene = scene;
    strcpy(meshNode->m_name, mesh->name.c_str());
    meshNode->meshInst = createMeshInstance(scene, mesh);
    scene->sceneNodes.push_back(meshNode);
    return meshNode;
}

void SceneManager::createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity)
{
#if 0
    CYAN_ASSERT(scene->dLights.size() < Scene::kMaxNumDirectionalLights, "Too many directional lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "DirLight%u", (u32)scene->dLights.size());
    Entity* entity = createEntity(scene, nameBuff, Transform(), true);
    DirectionalLight light(entity, glm::vec4(color, intensity), glm::vec4(direction, 0.f));
    scene->dLights.push_back(light);
#endif
}

void SceneManager::createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity)
{
    CYAN_ASSERT(scene->pointLights.size() < Scene::kMaxNumPointLights, "Too many point lights created.")
    char nameBuff[64];
    sprintf_s(nameBuff, "PointLight%u", (u32)scene->pointLights.size());
    Transform transform = Transform();
    transform.m_translate = glm::vec3(position);
    transform.m_scale = glm::vec3(0.1f);
    Entity* entity = createEntity(scene, nameBuff, Transform(), false); 
    Cyan::Mesh* sphereMesh = Cyan::AssetManager::getAsset<Cyan::Mesh>("sphere_mesh");
    CYAN_ASSERT(sphereMesh, "sphere_mesh does not exist")
    SceneComponent* meshNode = SceneManager::get()->createMeshNode(scene, transform, sphereMesh); 
    entity->m_sceneRoot->attachChild(meshNode);
    Cyan::Shader* pointLightShader = Cyan::ShaderManager::createShader({ Cyan::ShaderType::kVsPs, "PointLightShader", "../../shader/shader_light.vs", "../../shader/shader_light.fs" });
    // Cyan::MaterialInstance* matl = Cyan::createMaterial(pointLightShader)->createInstance();
    // meshNode->m_meshInstance->setMaterial(0, matl);
    glm::vec4 u_color = glm::vec4(color, intensity);
    // matl->set("color", &u_color.x);

    PointLight light(entity, glm::vec4(color, intensity), glm::vec4(position, 1.f));
    scene->pointLights.push_back(light);
}

void SceneManager::updateSceneGraph(Scene* scene)
{
    std::queue<SceneComponent*> nodes;
    nodes.push(scene->g_sceneRoot);
    while (!nodes.empty())
    {
        auto node = nodes.front();
        nodes.pop();
        if (node->m_parent)
        {
            glm::mat4& parentGlobalMatrix = scene->globalTransformMatrixPool.getObject(node->m_parent->globalTransform);
            glm::mat4& globalMatrix = scene->globalTransformMatrixPool.getObject(node->globalTransform);
            glm::mat4& localMatrix = scene->localTransformMatrixPool.getObject(node->localTransform);
            globalMatrix = parentGlobalMatrix * localMatrix;
            scene->globalTransformPool.getObject(node->globalTransform).fromMatrix(globalMatrix);
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

    // update scene's bounding box in world space
    for (auto entity : scene->entities)
    {
        std::queue<SceneComponent*> nodes;
        nodes.push(entity->m_sceneRoot);
        while (!nodes.empty())
        {
            SceneComponent* node = nodes.front();
            nodes.pop();
            for (auto child : node->m_child)
            {
                nodes.push(child);
            }
            if (Cyan::MeshInstance* meshInst = node->getAttachedMesh())
            {
                glm::mat4 model = node->getWorldTransform().toMatrix();
                BoundingBox3D aabb = meshInst->getAABB(model);
                scene->aabb.bound(aabb);
            }
        }
    }
    // scene->aabb.update();
}

Cyan::IrradianceProbe* SceneManager::createIrradianceProbe(Cyan::TextureRenderable* srcCubemapTexture, const glm::uvec2& irradianceRes)
{
    return new Cyan::IrradianceProbe(srcCubemapTexture, irradianceRes);
}

Cyan::IrradianceProbe* SceneManager::createIrradianceProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceResolution)
{
    return new Cyan::IrradianceProbe(scene, pos, sceneCaptureRes, irradianceResolution);
}

Cyan::ReflectionProbe* SceneManager::createReflectionProbe(Cyan::TextureRenderable* srcCubemapTexture)
{
    return new Cyan::ReflectionProbe(srcCubemapTexture);
}

Cyan::ReflectionProbe* SceneManager::createReflectionProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes)
{
    return new Cyan::ReflectionProbe(scene, pos, sceneCaptureRes);
}

glm::vec3 SceneManager::queryWorldPositionFromCamera(Scene* scene, const glm::vec2& uv)
{
    // generate a ray
    const auto& camera = scene->camera;
    glm::vec3 ro = camera.position;
    glm::vec3 rd = glm::normalize(uv.x * camera.right + uv.y * camera.up + camera.n * camera.forward);
    auto hit = scene->castRay(ro, rd, EntityFilter::kAll, true);
    return ro + hit.t * rd;
}

Cyan::MeshInstance* SceneManager::createMeshInstance(Scene* scene, Cyan::Mesh* mesh)
{
    scene->meshInstances.emplace_back();
    auto& meshInst = scene->meshInstances.back();
    meshInst = std::make_shared<Cyan::MeshInstance>(mesh);
    // attach a default material to all submeshes
    meshInst->setMaterial(Cyan::AssetManager::getDefaultMaterial());
    return meshInst.get();
}

Cyan::MeshInstance* SceneManager::createMeshInstance(Scene* scene, const char* meshName)
{
    auto assetManager = Cyan::AssetManager::get();
    auto mesh = assetManager->getAsset<Cyan::Mesh>(meshName);
    if (!mesh)
    {
        return nullptr;
    }
    return createMeshInstance(scene, mesh);
}
