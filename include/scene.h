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
    Camera mainCamera;
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
};

class SceneManager {
public:
    static void updateSceneGraph(Scene* scene);
    static void buildLightList(Scene* scene, std::vector<PointLightData>& pLights, std::vector<DirLightData>& dLights);
    static void setLightProbe(Scene* scene, LightProbe* probe);
    static void createDirectionalLight(Scene* scene, glm::vec3 color, glm::vec3 direction, float intensity);
    static void createPointLight(Scene* scene, glm::vec3 color, glm::vec3 position, float intensity);
    static Entity* createEntity(Scene* scene, const char* entityName, Transform transform, Entity* parent=nullptr);
    static Entity* getEntity(Scene* scene, u32 id) 
    {
        return scene->entities[id];
    }
    static Entity* getEntity(Scene* scene, const char* name)
    {
        for (auto& entity : scene->entities) {
            if (strcmp(name, entity->m_name) == 0) {
                return entity;
            }
        }
        return nullptr;
    }
};

extern SceneManager sceneManager;