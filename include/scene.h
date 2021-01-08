#pragma once

#include <vector>
#include "glm.hpp"

#include "Camera.h"
#include "Texture.h"
#include "Material.h"
#include "Entity.h"
#include "Light.h"

struct Scene 
{
    Camera mainCamera;
    Skybox skybox;
    std::vector<Entity> entities;
    std::vector<PointLight> pLights;
    std::vector<DirectionalLight> dLights;
};

class SceneManager {
public:
    static void setupSkybox(Skybox& skybox, CubemapTexture& texture);
    static void loadMesh(Mesh& mesh, const char* fileName);
    //static void loadSceneFromFile(Scene& scene, const char* fileName);
    static void loadTextureFromFile(Scene& scene, Texture& texture);
    static int findTextureIndex(const Scene& scene, const std::string& textureName);
    static void findTexturesForMesh(Scene& scene, Mesh& mesh);

    static Entity* createEntity(Scene& scene);
};

extern SceneManager sceneManager;