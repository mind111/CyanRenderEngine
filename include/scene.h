#pragma once

#include <vector>
#include "glm.hpp"
#include "mesh.h"
#include "camera.h"
#include "texture.h"

struct Light {
    glm::vec3 color;
    float intensity;
};

struct DirectionLight : Light {
    glm::vec3 direction;
};

struct PointLight : Light {
    glm::vec3 position;
    float attenuation;
};

struct Transform {
    glm::vec3 scale;
    glm::vec3 rotation; // not sure how to handle rotation for now
    glm::vec3 translation;
};

struct Scene {
    // Mesh
    Skybox skybox;
    std::vector<Mesh> meshList;
    std::vector<Transform> xformList;
    std::vector<MeshInstance> instanceList;

    // Textures
    std::vector<Texture> textureList;
    std::vector<CubemapTexture> cubemapTextures;

    Camera mainCamera;
    // Lights
    std::vector<PointLight> pLights;
    std::vector<DirectionLight> dLights;
};

class SceneManager {
public:
    static void setupSkybox(Skybox& skybox, CubemapTexture& texture);
    static void loadMesh(Mesh& mesh, const char* fileName);
    static void loadSceneFromFile(Scene& scene, const char* fileName);
    static void loadTextureFromFile(Scene& scene, Texture& texture);
    static int findTextureIndex(const Scene& scene, const std::string& textureName);
    static void findTexturesForMesh(Scene& scene, Mesh& mesh);
};

extern SceneManager sceneManager;