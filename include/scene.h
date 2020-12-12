#pragma once

#include <vector>
#include "glm.hpp"

#include "camera.h"
#include "Texture.h"
#include "Material.h"
#include "Entity.h"

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

// TODO: Scene should only own mesh instances and lights
struct Scene {
    Camera mainCamera;
    Skybox skybox;
    std::vector<Entity> entities;
    // Lights
    std::vector<PointLight> pLights;
    std::vector<DirectionLight> dLights;

    // legacy code to be removed soon
    /*
    // Mesh
    std::vector<Mesh> meshes;
    std::vector<Transform> xforms;
    std::vector<MeshInstance> instances;
    */


    /*
    // Textures
    std::vector<Texture> textures;
    std::vector<CubemapTexture> cubemapTextures;

    // Materials
    std::vector<Material> materials;
    */
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