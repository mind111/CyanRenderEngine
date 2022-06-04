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

struct Scene 
{
    Scene() 
    { 
        // create a default empty scene
    }

    static const u32 kMaxNumDirectionalLights = 1u;
    static const u32 kMaxNumPointLights = 20u;
    static const u32 kMaxNumSceneComponents = 1024u;

    // identifier
    std::string                             m_name;
    Camera                                  camera;
    Entity*                                 m_rootEntity;
    std::vector<Entity*>                    entities;
    // data
    SceneComponent*                              g_sceneRoot;
    u32                                     m_numSceneNodes;
    std::vector<SceneComponent*> sceneNodes;
    Cyan::ObjectPool<SceneComponent, 1024> sceneComponentPool;
    Cyan::ObjectPool<MeshComponent, 1024> meshComponentPool;
    Cyan::ObjectPool<Transform, 1024> localTransformPool;
    Cyan::ObjectPool<Transform, 1024> globalTransformPool;
    Cyan::ObjectPool<glm::mat4, 1024> localTransformMatrixPool;
    Cyan::ObjectPool<glm::mat4, 1024> globalTransformMatrixPool;

    std::vector<std::shared_ptr<Cyan::MeshInstance>> meshInstances;

    // lighting
    std::vector<PointLight>                 pointLights;
    Cyan::DirectionalLight                  directionalLight;
    Cyan::Skybox*                           m_skybox;
    Cyan::IrradianceProbe*                  m_irradianceProbe;
    Cyan::ReflectionProbe*                  m_reflectionProbe;
    // aabb
    BoundingBox3D                           aabb;

    RayCastInfo   castRay(glm::vec3& ro, glm::vec3& rd, EntityFilter filter, bool debugPrint=false);
    bool          castVisibilityRay(const glm::vec3& ro, glm::vec3& rd, EntityFilter filter);
};

class SceneManager 
{
public:
    SceneManager();
    static SceneManager* get();
    u32        allocEntityId();
    void       updateSceneGraph(Scene* scene);
    void       createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity);
    void       createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity);
    u32        allocSceneNode(Scene* scene);
    std::shared_ptr<Scene> importScene(const char* file, const char* name);
    SceneComponent* createSceneNode(Scene* scene, const char* name, Transform transform);
    MeshComponent* createMeshNode(Scene* scene, Transform transform, Cyan::Mesh* mesh);
    Entity*    createEntity(Scene* scene, const char* entityName, Transform transform, bool isStatic, Entity* parent=nullptr);
    Entity*    getEntity(Scene* scene, u32 id)
    {
        return scene->entities[id];
    }
    Entity* getEntity(Scene* scene, const char* name)
    {
        for (auto& entity : scene->entities) {
            if (strcmp(name, entity->m_name) == 0) {
                return entity;
            }
        }
        return nullptr;
    }
    u32 allocEntityId(Scene* scene)
    {
        return scene->entities.size() > 0 ? scene->entities.size() : 0;
    }

    // mesh instance
    Cyan::MeshInstance* createMeshInstance(Scene* scene, Cyan::Mesh* mesh);
    Cyan::MeshInstance* createMeshInstance(Scene* scene, const char* meshName);

    // material instance
    template <typename MaterialType>
    MaterialType* createMaterial(const char* name)
    {
        return new MaterialType(name);
    }

    // light probes
    Cyan::IrradianceProbe* createIrradianceProbe(Cyan::TextureRenderable* srcCubemapTexture, const glm::uvec2& irradianceRes);
    Cyan::IrradianceProbe* createIrradianceProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceRes);
    Cyan::ReflectionProbe* createReflectionProbe(Cyan::TextureRenderable* srcCubemapTexture);
    Cyan::ReflectionProbe* createReflectionProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes);

    /*
    * Return world position hit by a ray construct using uv coordinates on camera's image plane.
    * 'uv' should be in [-1, 1]
    */
    glm::vec3 queryWorldPositionFromCamera(Scene* scene, const glm::vec2& uv);

private:
    static SceneManager* s_sceneManager;
};

namespace Cyan
{
}