#pragma once

#include <vector>
#include <queue>
#include "glm.hpp"

#include "Camera.h"
#include "Texture.h"
#include "Light.h"
#include "Mesh.h"
#include "Material.h"
#include "LightProbe.h"
#include "SkyBox.h"

struct Scene 
{
    static const u32 kMaxNumPointLights = 20u;
    static const u32 kMaxNumDirLights   = 20u;
    static const u32 kMaxNumSceneNodes = 1024u;
    // identifier
    std::string                             m_name;
    u32                                     activeCamera;
    Camera                                  cameras[2];
    Entity*                                 m_rootEntity;
    std::vector<Entity*>                    entities;
    // data
    SceneNode*                              g_sceneRoot;
    u32                                     m_numSceneNodes;
    std::vector<SceneNode>                  g_sceneNodes;
    std::vector<Transform>                  g_localTransforms;
    std::vector<Transform>                  g_globalTransforms;
    std::vector<glm::mat4>                  g_localTransformMatrices;
    std::vector<glm::mat4>                  g_globalTransformMatrices;

    // todo: these resources should be managed by SceneManager instead, only mesh and material instances need to be managed by scene
    std::vector<Cyan::Texture>              g_textures;
    std::vector<Cyan::MeshInstance>         meshInstances;

    // lighting
    std::vector<PointLight>                 pointLights;
    std::vector<DirectionalLight>           dLights;
    Cyan::Skybox*                           m_skybox;
    Cyan::IrradianceProbe*                  m_irradianceProbe;
    Cyan::ReflectionProbe*                  m_reflectionProbe;
    // aabb
    BoundingBox3D                           aabb;

    Camera& getActiveCamera()
    {
        return cameras[activeCamera];
    }

    Camera& getCamera(u32 index)
    {
        return cameras[index];
    }

    RayCastInfo   castRay(glm::vec3& ro, glm::vec3& rd, EntityFilter filter, bool debugPrint=false);
    bool          castVisibilityRay(const glm::vec3& ro, glm::vec3& rd, EntityFilter filter);
    // BoundingBox3D getBoundingBox();
};

struct RayTracingScene
{
    // todo: triangles
    // todo: materials
};

class SceneManager {
public:
    SceneManager();
    static SceneManager* getSingletonPtr();
    u32        allocEntityId();
    void       updateSceneGraph(Scene* scene);
    void       createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity);
    void       createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity);
    u32        allocSceneNode(Scene* scene);
    Scene*     createScene(const char* file, const char* name);
    SceneNode* createSceneNode(Scene* scene, const char* name, Transform transform);
    MeshNode* createMeshNode(Scene* scene, Transform transform, Cyan::Mesh* mesh);
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
    Cyan::IrradianceProbe* createIrradianceProbe(Cyan::Texture* srcCubemapTexture, const glm::uvec2& irradianceRes);
    Cyan::IrradianceProbe* createIrradianceProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes, const glm::uvec2& irradianceRes);
    Cyan::ReflectionProbe* createReflectionProbe(Cyan::Texture* srcCubemapTexture);
    Cyan::ReflectionProbe* createReflectionProbe(Scene* scene, const glm::vec3& pos, const glm::uvec2& sceneCaptureRes);

    /*
    * Return world position hit by a ray construct using uv coordinates on camera's image plane.
    * 'uv' should be in [-1, 1]
    */
    glm::vec3 queryWorldPositionFromCamera(Scene* scene, const glm::vec2& uv);

private:
    static SceneManager*     s_sceneManager;
    SceneNodeFactory<Cyan::PoolAllocator<SceneNode, 1024>> m_sceneNodeFactory;
    MeshNodeFactory<Cyan::PoolAllocator<MeshNode, 1024>> m_meshNodeFactory;
};