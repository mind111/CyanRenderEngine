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

namespace Cyan
{
    Scene::Scene(const char* inSceneName, f32 cameraAspectRatio)
        : name(inSceneName)
    {
        rootEntity = createEntity("Root", Transform());
        camera = std::unique_ptr<CameraEntity>(createPerspectiveCamera(
            /*name=*/"Camera",
            /*transform=*/Transform {
                glm::vec3(0.f, 3.2f, 8.f)
            },
            /*inLookAt=*/glm::vec3(0., 1., -1.),
            /*inWorldUp=*/glm::vec3(0.f, 1.f, 0.f),
            /*inFov=*/45.f,
            .1f,
            150.f,
            cameraAspectRatio
        ));
    }

    void Scene::update()
    {
        camera->update();

        std::queue<SceneComponent*> nodes;
        nodes.push(rootEntity->getRootSceneComponent());
        while (!nodes.empty())
        {
            auto node = nodes.front();
            nodes.pop();
            if (node->parent)
            {
                glm::mat4& parentGlobalMatrix = globalTransformMatrixPool.getObject(node->parent->globalTransform);
                glm::mat4& globalMatrix = globalTransformMatrixPool.getObject(node->globalTransform);
                glm::mat4& localMatrix = localTransformMatrixPool.getObject(node->localTransform);
                globalMatrix = parentGlobalMatrix * localMatrix;
                globalTransformPool.getObject(node->globalTransform).fromMatrix(globalMatrix);
                globalTransformMatrixPool.getObject(node->globalTransform) = globalMatrix;
            }
            for (u32 i = 0; i < node->childs.size(); ++i)
            {
                nodes.push(node->childs[i]);
            }
            for (u32 i = 0; i < node->indirectChilds.size(); ++i)
            {
                nodes.push(node->indirectChilds[i]);
            }
        }

        // update scene's bounding box in world space
        for (auto entity : entities)
        {
            std::queue<SceneComponent*> nodes;
            nodes.push(entity->getRootSceneComponent());
            while (!nodes.empty())
            {
                SceneComponent* node = nodes.front();
                nodes.pop();
                for (auto child : node->childs)
                {
                    nodes.push(child);
                }
                if (Cyan::MeshInstance* meshInst = node->getAttachedMesh())
                {
                    glm::mat4 model = node->getWorldTransform().toMatrix();
                    BoundingBox3D aabb = meshInst->getAABB(model);
                    aabb.bound(aabb);
                }
            }
        }
    }

    SceneComponent* Scene::createSceneComponent(const char* name, Transform transform)
    {
        SceneComponent* sceneComponent = sceneComponentPool.alloc();

        Transform* localTransform = localTransformPool.alloc();
        *localTransform = transform;
        auto localTransformMatrix = localTransformMatrixPool.alloc();
        *localTransformMatrix = transform.toMatrix();
        sceneComponent->localTransform = localTransformPool.getObjectIndex(localTransform);

        Transform* globalTransfom = globalTransformPool.alloc();
        *globalTransfom = Transform();
        auto globalTransformMatrix = globalTransformMatrixPool.alloc();
        *globalTransformMatrix = glm::mat4(1.f);
        sceneComponent->globalTransform = globalTransformPool.getObjectIndex(globalTransfom);

        sceneComponent->m_scene = this;
        sceneComponent->name = name;
        sceneComponents.push_back(sceneComponent);
        return sceneComponent;
    }

    MeshComponent* Scene::createMeshComponent(Cyan::Mesh* mesh, Transform transform)
    {
        MeshComponent* meshComponent = meshComponentPool.alloc();

        Transform* localTransform = localTransformPool.alloc();
        *localTransform = transform;
        auto localTransformMatrix = localTransformMatrixPool.alloc();
        *localTransformMatrix = transform.toMatrix();
        meshComponent->localTransform = localTransformPool.getObjectIndex(localTransform);

        Transform* globalTransfom = globalTransformPool.alloc();
        *globalTransfom = Transform();
        auto globalTransformMatrix = globalTransformMatrixPool.alloc();
        *globalTransformMatrix = glm::mat4(1.f);
        meshComponent->globalTransform = globalTransformPool.getObjectIndex(globalTransfom);

        meshComponent->m_scene = this;
        meshComponent->name = mesh->name;
        meshComponent->meshInst = createMeshInstance(mesh);
        sceneComponents.push_back(meshComponent);
        return meshComponent;
    }

    Entity* Scene::createEntity(const char* name, const Transform& transform, Entity* inParent, u32 properties)
    {
        Entity* entity = new Entity(this, name, transform, inParent, properties);
        entities.push_back(entity);
        return entity;
    }

    StaticMeshEntity* Scene::createStaticMeshEntity(const char* name, const Transform& transform, Mesh* inMesh, Entity* inParent, u32 properties)
    {
        auto staticMesh = new StaticMeshEntity(this, name, transform, inMesh, inParent, properties);
        entities.push_back(staticMesh);
        return staticMesh;
    }

    CameraEntity* Scene::createPerspectiveCamera(const char* name, const Transform& transform, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio, Entity* inParent, u32 properties)
    {
        auto camera = new CameraEntity(this, name, transform, inLookAt, inWorldUp, inFov, inN, inF, inAspectRatio, inParent, properties);
        entities.push_back(camera);
        return camera;
    }

    DirectionalLightEntity* Scene::createDirectionalLight(const char* name, const glm::vec3& direction, const glm::vec4& colorAndIntensity)
    {
        DirectionalLightEntity* directionalLight = new DirectionalLightEntity(this, name, Transform(), nullptr, direction, colorAndIntensity, true);
        entities.push_back(directionalLight);
        return directionalLight;
    }

    SkyLight* Scene::createSkyLight(const char* name, const glm::vec4& colorAndIntensity)
    {
        if (skyLight)
        {
            cyanError("There is already a skylight exist in scene %s", this->name);
            return nullptr;
        }
    }

    SkyLight* Scene::createSkyLightFromSkybox(Skybox* srcSkybox) {
        return nullptr;
    }

    SkyLight* Scene::createSkyLight(const char* name, const char* srcHDRI) 
    { 
        if (skyLight)
        {
            cyanError("There is already a skylight exist in scene %s", this->name);
            return nullptr;
        }
        skyLight = new SkyLight(this, glm::vec4(1.f), srcHDRI);
        return skyLight;
    }

    Skybox* Scene::createSkybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution) {
        if (skybox) {
            cyanError("There is already a skybox exist in scene %s", this->name);
            return nullptr;
        }
        skybox = new Skybox(name, srcHDRIPath, resolution);
        return skybox;
    }

    void Scene::createPointLight(const char* name, const glm::vec3 position, const glm::vec4& colorAndIntensity)
    {

    }

    IrradianceProbe* Scene::createIrradianceProbe(Cyan::TextureCubeRenderable* srcCubemapTexture, const glm::uvec2& irradianceRes)
    {
        return new IrradianceProbe(srcCubemapTexture, irradianceRes);
    }

    IrradianceProbe* Scene::createIrradianceProbe(const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceResolution) {
        return new IrradianceProbe(this, pos, sceneCaptureRes, irradianceResolution);
    }

    ReflectionProbe* Scene::createReflectionProbe(Cyan::TextureCubeRenderable* srcCubemapTexture) {
        return new ReflectionProbe(srcCubemapTexture);
    }

    ReflectionProbe* Scene::createReflectionProbe(const glm::vec3& pos, const glm::uvec2& sceneCaptureRes) {
        return new ReflectionProbe(this, pos, sceneCaptureRes);
    }

    MeshInstance* Scene::createMeshInstance(Cyan::Mesh* mesh) {
        meshInstances.emplace_back();
        auto& meshInst = meshInstances.back();
        meshInst = std::make_shared<MeshInstance>(mesh);
        // attach a default material to all submeshes
        meshInst->setMaterial(AssetManager::getAsset<Material>("DefaultMaterial"));
        return meshInst.get();
    }

    Cyan::MeshInstance* Scene::createMeshInstance(const char* meshName)
    {
        auto assetManager = Cyan::AssetManager::get();
        auto mesh = assetManager->getAsset<Cyan::Mesh>(meshName);
        if (!mesh)
        {
            return nullptr;
        }
        return createMeshInstance(mesh);
    }

    SceneManager* Singleton<SceneManager>::singleton = nullptr;

    SceneManager::SceneManager()
        : Singleton<SceneManager>()
    { }

    std::shared_ptr<Scene> SceneManager::importScene(const char* name, const char* filePath)
    {
#if 0
        Cyan::Toolkit::GpuTimer loadSceneTimer("SceneManager::importScene()", true);
        std::shared_ptr<Scene> scene = std::make_shared<Scene>(name);
        auto assetManager = AssetManager::get();
        assetManager->importScene(scene.get(), filePath);
        loadSceneTimer.end();
        return scene;
#endif
        return nullptr;
    }
}
