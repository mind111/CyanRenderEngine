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

// TODO: implement this
struct LightingEnvironment
{
    std::vector<PointLight>& m_pLights;
    std::vector<DirectionalLight>& m_dirLights;
    LightProbe* m_probe;
    Cyan::IrradianceProbe* m_irradianceProbe;
    bool bUpdateProbeData;
};

struct Scene 
{
    static const u32 kMaxNumPointLights = 20u;
    static const u32 kMaxNumDirLights = 20u;
    // identifier
    std::string    m_name;
    // camera
    u32 activeCamera;
    Camera cameras[2];
    // entities
    Entity*     m_rootEntity;
    std::vector<Entity*> entities;
    // lighting
    u32 m_currentEnvMap;
    std::vector<PointLight> pLights;
    std::vector<DirectionalLight> dLights;
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;
    Cyan::IrradianceProbe* m_irradianceProbe;
    LightProbe*    m_currentProbe;
    LightProbe*    m_lastProbe;

    Camera& getActiveCamera()
    {
        return cameras[activeCamera];
    }

    Camera& getCamera(u32 index)
    {
        return cameras[index];
    }

    RayCastInfo castRay(glm::vec3& ro, glm::vec3& rd, EntityFilter filter, bool debugPrint=false);
    bool castVisibilityRay(glm::vec3& ro, glm::vec3& rd);
};

class SceneManager {
public:
    SceneManager();
    static SceneManager* getSingletonPtr();
    u32 allocEntityId();
    // TODO: implement this
    std::vector<Entity*> packEntities() { }
    void updateSceneGraph(Scene* scene);
    void buildLightList(Scene* scene, std::vector<PointLightGpuData>& pLights, std::vector<DirLightGpuData>& dLights);
    void setLightProbe(Scene* scene, LightProbe* probe);
    void createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity);
    void createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity);
    Entity* createEntity(Scene* scene, const char* entityName, Transform transform, Entity* parent=nullptr);
    Entity* getEntity(Scene* scene, u32 id) 
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
    //- probe related
    Cyan::IrradianceProbe* createIrradianceProbe(Scene* scene, glm::vec3& pos);
    Cyan::LightFieldProbe* createLightFieldProbe(Scene* scene, glm::vec3& pos);
    Cyan::LightFieldProbeVolume* createLightFieldProbeVolume(Scene* scene, glm::vec3& pos, glm::vec3& dimension, glm::vec3& stride);
private:
    static SceneManager* s_sceneManager;
    Cyan::LightProbeFactory* m_probeFactory;
};