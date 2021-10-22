#pragma once

#include <vector>
#include <queue>
#include "glm.hpp"

#include "Camera.h"
#include "Texture.h"
#include "Light.h"
#include "Material.h"
#include "LightProbe.h"

// TODO: implement this
struct LightingEnvironment
{
    std::vector<PointLight> pLights;
    std::vector<DirectionalLight> dLights;
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
};

class SceneManager {
public:
    SceneManager();
    static SceneManager* getSingletonPtr();
    u32 allocEntityId();
    void updateSceneGraph(Scene* scene);
    void buildLightList(Scene* scene, std::vector<PointLightData>& pLights, std::vector<DirLightData>& dLights);
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
    Cyan::IrradianceProbe* createIrradianceProbe(Scene* scene, glm::vec3& pos);
    Cyan::LightFieldProbe* createLightFieldProbe(Scene* scene, glm::vec3& pos);
private:
    static SceneManager* s_sceneManager;
    Cyan::LightProbeFactory* m_probeFactory;
};