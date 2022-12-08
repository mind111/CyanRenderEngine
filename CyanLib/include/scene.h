#pragma once

#include <vector>
#include <queue>
#include <stack>
#include <memory>

#include "glm.hpp"

#include "Allocator.h"
#include "Camera.h"
#include "Texture.h"
#include "LightEntities.h"
#include "Mesh.h"
#include "Material.h"
#include "LightProbe.h"
#include "SkyBox.h"
#include "SkyLight.h"
#include "StaticMeshEntity.h"

namespace Cyan
{
    struct SceneComponent;

    struct Scene {
        Scene(const char* sceneName, f32 cameraAspectRatio);

        void update();

        // entities
        Entity* createEntity(const char* name, const Transform& transform, Entity* inParent = nullptr, u32 properties = (EntityFlag_kDynamic | EntityFlag_kVisible | EntityFlag_kCastShadow));
        StaticMeshEntity* createStaticMeshEntity(const char* name, const Transform& transform, Mesh* inMesh, Entity* inParent = nullptr, u32 properties = (EntityFlag_kDynamic | EntityFlag_kVisible | EntityFlag_kCastShadow));
        // scene component
        SceneComponent* createSceneComponent(const char* name, Transform transform);
        MeshComponent* createMeshComponent(Mesh* mesh, Transform transform);
        // mesh instance
        MeshInstance* createMeshInstance(Cyan::Mesh* mesh);
        MeshInstance* createMeshInstance(const char* meshName);
        // camera
        CameraEntity* createPerspectiveCamera(const char* name, const Transform& transform, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, f32 inFov, f32 inN, f32 inF, f32 inAspectRatio, Entity* inParent = nullptr, u32 properties = (EntityFlag_kDynamic));

        // lights
        /**
        * sun light
        */
        DirectionalLightEntity* createDirectionalLight(const char* name, const glm::vec3& direction, const glm::vec4& colorAndIntensity);

        /**
        * sky light
        */
        SkyLight* createSkyLight(const char* name, const glm::vec4& colorAndIntensity);
        SkyLight* createSkyLight(const char* name, const char* srcHDRI);
        SkyLight* createSkyLightFromSkybox(Skybox* srcSkybox);
        void createPointLight(const char* name, const glm::vec3 position, const glm::vec4& colorAndIntensity);
        IrradianceProbe* createIrradianceProbe(Cyan::TextureCubeRenderable* srcCubemapTexture, const glm::uvec2& irradianceRes);
        IrradianceProbe* createIrradianceProbe(const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceRes);
        ReflectionProbe* createReflectionProbe(Cyan::TextureCubeRenderable* srcCubemapTexture);
        ReflectionProbe* createReflectionProbe(const glm::vec3& pos, const glm::uvec2& sceneCaptureRes);

        // skybox
        Skybox* createSkybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution);

        // getters
        Entity* getEntity(u32 index) { return entities[index]; }
        Entity* getEntity(const char* name) {
            for (auto& entity : entities) {
                if (strcmp(name, entity->name.c_str()) == 0) {
                    return entity;
                }
            }
            return nullptr;
        }

        static const u32 kMaxNumDirectionalLights = 1u;
        static const u32 kMaxNumPointLights = 32u;
        static const u32 kMaxNumSceneComponents = 2048u;

        std::string name;
        BoundingBox3D aabb;
        Entity* rootEntity = nullptr;
        std::vector<Entity*> entities;

        /**
        * Currently active camera that will be used to render the scene, a scene can potentially
        * has multiple cameras but only one will be active at any moment.
        */
        std::unique_ptr<CameraEntity> camera = nullptr;

        std::vector<std::shared_ptr<MeshInstance>> meshInstances;
        std::vector<SceneComponent*> sceneComponents;

        // resource pools
        ObjectPool<SceneComponent, kMaxNumSceneComponents> sceneComponentPool;
        ObjectPool<MeshComponent, kMaxNumSceneComponents> meshComponentPool;
        ObjectPool<Transform, kMaxNumSceneComponents> localTransformPool;
        ObjectPool<Transform, kMaxNumSceneComponents> globalTransformPool;
        ObjectPool<glm::mat4, kMaxNumSceneComponents> localTransformMatrixPool;
        ObjectPool<glm::mat4, kMaxNumSceneComponents> globalTransformMatrixPool;

        SkyLight* skyLight = nullptr;
        Skybox* skybox = nullptr;
    };

    class SceneManager : public Singleton<SceneManager>
    {
    public:
        SceneManager();

        std::shared_ptr<Scene> importScene(const char* file, const char* name);
        std::shared_ptr<Scene> createScene(const char* name) { }
    private:
    };
}