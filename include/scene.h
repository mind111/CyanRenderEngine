#pragma once

#include <vector>
#include "glm.hpp"

#include "Camera.h"
#include "Texture.h"
#include "Entity.h"
#include "Light.h"

struct Scene 
{
    Camera mainCamera;
    std::vector<Entity> entities;
    std::vector<PointLight> pLights;
    std::vector<DirectionalLight> dLights;
};

class SceneManager {
public:
    static void createDirectionalLight(Scene& scene, glm::vec3 color, glm::vec3 direction, float intensity);
    static void createPointLight(Scene& scene, glm::vec3 color, glm::vec3 position, float intensity);
    static Entity* createEntity(Scene& scene);
};

extern SceneManager sceneManager;