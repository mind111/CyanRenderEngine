#pragma once

#include <vector>
#include <queue>
#include "glm.hpp"

#include "Camera.h"
#include "Texture.h"
#include "Light.h"
#include "Material.h"
#include "LightProbe.h"

struct SkyLight
{

};

struct LightingEnvironment
{
    std::vector<PointLight>&       m_pLights;
    std::vector<DirectionalLight>& m_dirLights;
    DistantLightProbe*             m_distantProbe;
    Cyan::IrradianceProbe*         irradianceProbe;
    Cyan::ReflectionProbe*         localReflectionProbe;
};

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
    std::vector<Cyan::StandardPbrMaterial*> m_materials;
    std::vector<Transform>                  g_localTransforms;
    std::vector<Transform>                  g_globalTransforms;
    std::vector<glm::mat4>                  g_localTransformMatrices;
    std::vector<glm::mat4>                  g_globalTransformMatrices;
    std::vector<Cyan::Material>             g_materials;
    std::vector<Cyan::MaterialInstance>     g_materialInstances;
    std::vector<Cyan::Mesh>                 g_meshes;
    std::vector<Cyan::Texture>              g_textures;
    // lighting
    u32                                     m_currentEnvMap;
    std::vector<PointLight>                 pLights;
    std::vector<DirectionalLight>           dLights;
    Cyan::IrradianceProbe*                  m_irradianceProbe;
    Cyan::ReflectionProbe*                  m_reflectionProbe;
    DistantLightProbe*                      m_distantProbe;

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
    void          addStandardPbrMaterial(Cyan::StandardPbrMaterial* matl);
    BoundingBox3f getBoundingBox();
};

class SceneManager {
public:
    SceneManager();
    static SceneManager* getSingletonPtr();
    u32        allocEntityId();
    void       updateSceneGraph(Scene* scene);
    void       buildLightList(Scene* scene, std::vector<PointLightGpuData>& pLights, std::vector<DirLightGpuData>& dLights);
    void       setDistantLightProbe(Scene* scene, DistantLightProbe* probe);
    void       createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity);
    void       createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity);
    u32        allocSceneNode(Scene* scene);
    Scene*     createScene(const char* file, const char* name);
    SceneNode* createSceneNode(Scene* scene, const char* name, Transform transform, Cyan::Mesh* mesh, bool hasAABB=true);
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

    void traverseScene(Scene* scene);
    //- probe related
    Cyan::IrradianceProbe* createIrradianceProbe(Scene* scene, const glm::vec3& pos);
    Cyan::ReflectionProbe* createReflectionProbe(Scene* scene, const glm::vec3& pos);
    Cyan::LightFieldProbe* createLightFieldProbe(Scene* scene, const glm::vec3& pos);
    Cyan::LightFieldProbeVolume* createLightFieldProbeVolume(Scene* scene, glm::vec3& pos, glm::vec3& dimension, glm::vec3& stride);
private:
    //u32                      m_numSceneNodes;
    //std::vector<SceneNode>   m_sceneNodePool;
    static SceneManager*     s_sceneManager;
    Cyan::LightProbeFactory* m_probeFactory;
};