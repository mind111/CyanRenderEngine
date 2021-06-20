#pragma once

#include <vector>
#include "glm.hpp"

#include "Camera.h"
#include "Texture.h"
#include "Entity.h"
#include "Light.h"
#include "Material.h"
#include "LightProbe.h"

struct Scene 
{
    static const u32 kMaxNumPointLights = 20u;
    static const u32 kMaxNumDirLights = 20u;
    Camera mainCamera;
    u32 m_currentEnvMap;
    std::vector<Entity*> entities;
    std::vector<PointLight> pLights;
    std::vector<DirectionalLight> dLights;
    RegularBuffer* m_pointLightsBuffer;
    RegularBuffer* m_dirLightsBuffer;
    LightProbe*    m_currentProbe;
    LightProbe*    m_lastProbe;
};

class SceneManager {
public:
    static void setLightProbe(Scene* scene, LightProbe* probe);
    static void createDirectionalLight(Scene& scene, glm::vec3 color, glm::vec3 direction, float intensity);
    static void createPointLight(Scene& scene, glm::vec3 color, glm::vec3 position, float intensity);
    static Entity* createEntity(Scene* scene, const char* meshName);
    static Entity* createEntity(Scene* scene, const char* meshName, Transform transform);
    static Entity* getEntity(Scene* scene, u32 id) 
    {
        return scene->entities[id];
    }
};

extern SceneManager sceneManager;