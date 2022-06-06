#pragma once

#include <vector>
#include <queue>
#include <stack>
#include <memory>

#include "glm.hpp"

#include "Allocator.h"
#include "Camera.h"
#include "Texture.h"
#include "Light.h"
#include "Mesh.h"
#include "Material.h"
#include "LightProbe.h"
#include "SkyBox.h"
#include "DirectionalLight.h"

namespace Cyan
{
    struct Scene 
    {
        Scene(const char* sceneName);

        void update();

        // entities
        Entity* createEntity(const char* name, const Transform& transform, Entity* inParent = nullptr, u32 properties = (EntityFlag_kDynamic | EntityFlag_kVisible | EntityFlag_kCastShadow));
        SceneComponent* createSceneComponent(const char* name, Transform transform);
        MeshComponent* createMeshComponent(Transform transform, Cyan::Mesh* mesh);
        // mesh instance
        Cyan::MeshInstance* createMeshInstance(Cyan::Mesh* mesh);
        Cyan::MeshInstance* createMeshInstance(const char* meshName);
        // lights
        void createDirectionalLight(const char* name, const glm::vec3& direction, const glm::vec4& colorAndIntensity);
        void createPointLight(const char* name, const glm::vec3 position, const glm::vec4& colorAndIntensity);
        Cyan::IrradianceProbe* createIrradianceProbe(Cyan::TextureCubeRenderable* srcCubemapTexture, const glm::uvec2& irradianceRes);
        Cyan::IrradianceProbe* createIrradianceProbe(const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceRes);
        Cyan::ReflectionProbe* createReflectionProbe(Cyan::TextureCubeRenderable* srcCubemapTexture);
        Cyan::ReflectionProbe* createReflectionProbe(const glm::vec3& pos, const glm::uvec2& sceneCaptureRes);

        // getters
        Entity* getEntity(u32 index) { return entities[index]; }
        Entity* getEntity(const char* name)
        {
            for (auto& entity : entities) {
                if (strcmp(name, entity->name.c_str()) == 0) {
                    return entity;
                }
            }
            return nullptr;
        }

        static const u32 kMaxNumDirectionalLights = 1u;
        static const u32 kMaxNumPointLights = 32u;
        static const u32 kMaxNumSceneComponents = 1024u;

        std::string name;
        Camera camera;
        BoundingBox3D aabb;
        Entity* rootEntity = nullptr;
        std::vector<Entity*> entities;
        std::vector<std::shared_ptr<MeshInstance>> meshInstances;
        std::vector<SceneComponent*> sceneComponents;

        // resource pools
        Cyan::ObjectPool<SceneComponent, kMaxNumSceneComponents> sceneComponentPool;
        Cyan::ObjectPool<MeshComponent, kMaxNumSceneComponents> meshComponentPool;
        Cyan::ObjectPool<Transform, kMaxNumSceneComponents> localTransformPool;
        Cyan::ObjectPool<Transform, kMaxNumSceneComponents> globalTransformPool;
        Cyan::ObjectPool<glm::mat4, kMaxNumSceneComponents> localTransformMatrixPool;
        Cyan::ObjectPool<glm::mat4, kMaxNumSceneComponents> globalTransformMatrixPool;

        Cyan::DirectionalLight directionalLight;
        Cyan::Skybox* skybox = nullptr;
        Cyan::IrradianceProbe* irradianceProbe = nullptr;
        Cyan::ReflectionProbe* reflectionProbe = nullptr;
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