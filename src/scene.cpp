#include <iostream>
#include <fstream>
#include "scene.h"
#include "json.hpp"
#include "glm.hpp"
#include "stb_image.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "gtc/matrix_transform.hpp"

SceneManager sceneManager;

namespace glm {
    //---- Utilities for loading the scene data from json
    void from_json(const nlohmann::json& j, glm::vec3& v) { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }
}

void from_json(const nlohmann::json& j, Transform& t) {
    t.translation = j.at("translation").get<glm::vec3>();
    t.rotation = j.at("rotation").get<glm::vec3>();
    t.scale = j.at("scale").get<glm::vec3>();
}

void from_json(const nlohmann::json& j, Camera& c) {
    c.position = j.at("position").get<glm::vec3>();
    c.target = j.at("target").get<glm::vec3>();
    j.at("fov").get_to(c.fov);
    j.at("z_far").get_to(c.far);
    j.at("z_near").get_to(c.near);
}

void from_json(const nlohmann::json& j, Mesh& mesh) {
    j.at("name").get_to(mesh.name);
}
//-------------------------------------------------------

// Mesh loading
void SceneManager::loadMesh(Mesh& mesh, const char* fileName) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        fileName,
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals
        );

    for (int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* assimpMesh = scene->mMeshes[i];
        mesh.numVerts = assimpMesh->mNumVertices;

        //--------- Format conversion -------------------------------------
        float* verts = new float[assimpMesh->mNumVertices * 3]; 
        float* normals = new float[assimpMesh->mNumVertices * 3];
        float* textureCoords = new float[assimpMesh->mNumVertices * 2];
        float* tangents = new float[assimpMesh->mNumVertices * 3];
        float* bitangents = new float[assimpMesh->mNumVertices * 3];
        unsigned int* indices = new unsigned int[assimpMesh->mNumFaces * 3];

        for (int v = 0; v < assimpMesh->mNumVertices; v++) {
            verts[v * 3    ] = assimpMesh->mVertices[v].x;
            verts[v * 3 + 1] = assimpMesh->mVertices[v].y;
            verts[v * 3 + 2] = assimpMesh->mVertices[v].z;
        }
        //-----------------------------------------------------------------

        // -------- Upload mesh data to gpu -------------------------------
        glCreateVertexArrays(1, &mesh.vao);
        glBindVertexArray(mesh.vao);
        glGenBuffers(1, &mesh.posVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.posVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts[0]) * assimpMesh->mNumVertices * 3, verts, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        if (assimpMesh->HasFaces()) {
            for (int v = 0; v < assimpMesh->mNumVertices; v++) {
                indices[v] = assimpMesh->mFaces[v / 3].mIndices[v % 3];
            }
            glGenBuffers(1, &mesh.ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices[0]) * assimpMesh->mNumFaces * 3, indices, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }
        if (assimpMesh->HasNormals()) {
            for (int v = 0; v < assimpMesh->mNumVertices; v++) {
                normals[v * 3    ] = assimpMesh->mNormals[v].x;
                normals[v * 3 + 1] = assimpMesh->mNormals[v].y;
                normals[v * 3 + 2] = assimpMesh->mNormals[v].z;
            }
            glGenBuffers(1, &mesh.normalVBO);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.normalVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(normals[0]) * assimpMesh->mNumVertices * 3, normals, GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
            glEnableVertexAttribArray(1);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        // TODO:: right now only consider the first set of uv 
        if (assimpMesh->HasTextureCoords(0)) {
            for (int v = 0; v < assimpMesh->mNumVertices; v++) {
                textureCoords[v * 2] = assimpMesh->mTextureCoords[0][v].x;
                textureCoords[v * 2 + 1] = assimpMesh->mTextureCoords[0][v].y;
            }
            glGenBuffers(1, &mesh.textureCoordVBO);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.textureCoordVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(textureCoords[0]) * assimpMesh->mNumVertices * 2, textureCoords, GL_STATIC_DRAW);
            // TODO: Hard-code this to 2-components for now
            glVertexAttribPointer(2, 2, GL_FLOAT, false, 0, 0); // TODO: this may be problematic
            glEnableVertexAttribArray(2);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        if (assimpMesh->HasTangentsAndBitangents()) {
            for (int v = 0; v < assimpMesh->mNumVertices; v++) {
                tangents[v * 3    ] = assimpMesh->mTangents[v].x;
                tangents[v * 3 + 1] = assimpMesh->mTangents[v].y;
                tangents[v * 3 + 2] = assimpMesh->mTangents[v].z;
                bitangents[v * 3    ] = assimpMesh->mBitangents[v].x;
                bitangents[v * 3 + 1] = assimpMesh->mBitangents[v].y;
                bitangents[v * 3 + 2] = assimpMesh->mBitangents[v].z;
            }

            // Upload tangents
            glGenBuffers(1, &mesh.tangentVBO);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.tangentVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(tangents[0]) * assimpMesh->mNumVertices * 3, tangents, GL_STATIC_DRAW);
            glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
            glEnableVertexAttribArray(3);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            // Upload biTangents
            glGenBuffers(1, &mesh.biTangentVBO);
            glBindBuffer(GL_ARRAY_BUFFER, mesh.biTangentVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(bitangents[0]) * assimpMesh->mNumVertices * 3, bitangents, GL_STATIC_DRAW);
            glVertexAttribPointer(4, 3, GL_FLOAT, false, 0, 0);
            glEnableVertexAttribArray(4);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glBindVertexArray(0);
        // ---------------------------------------------------------------------

        // ----- Clean up -------------
        delete[] verts;
        delete[] normals;
        delete[] textureCoords;
        delete[] tangents;
        delete[] bitangents;
    }
}

void SceneManager::loadTextureFromFile(Scene& scene, Texture& texture) {
    int w, h, numChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* imageData = stbi_load(texture.path.c_str(), &w, &h, &numChannels, 0);

    //---- Upload to gpu ----
    glGenTextures(1, &texture.textureObj);
    glBindTexture(GL_TEXTURE_2D, texture.textureObj);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(imageData);

    //---- Texture params ----
    // TODO: look into each one's difference in detail
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void SceneManager::loadSceneFromFile(Scene& scene, const char* fileName) {
    nlohmann::json sceneJson;
    std::ifstream sceneFile(fileName);
    sceneFile >> sceneJson;
    auto cameras = sceneJson["cameras"];
    auto lights = sceneJson["lights"];
    auto meshInfoList = sceneJson["mesh_info_list"];
    auto textureInfoList = sceneJson["texture_info_list"];
    auto instanceInfoList = sceneJson["instance_list"];

    // TODO: each scene should only have one camera
    for (auto camera : cameras) {
        scene.mainCamera = camera.get<Camera>();
        scene.mainCamera.eMode = ControlMode::free;
        scene.mainCamera.view = glm::mat4(1.f);
        scene.mainCamera.projection = glm::perspective(glm::radians(scene.mainCamera.fov), 800.f / 600.f, scene.mainCamera.near, scene.mainCamera.far);
    }
    for (auto lightInfo : lights) {
        std::string lightType;
        DirectionLight directionLight;
        PointLight pointLight;
        lightInfo.at("type").get_to(lightType);
        if (lightType == "directional") {
            directionLight.direction = glm::normalize(lightInfo.at("direction").get<glm::vec3>());
            directionLight.color = lightInfo.at("color").get<glm::vec3>();
            scene.dLights.emplace_back(directionLight);
        } else {

        } // pointLight case
    }
    for (auto textureInfo : textureInfoList) {
        Texture texture = {
            textureInfo.at("textureName").get<std::string>(),
            textureInfo.at("texturePath").get<std::string>(),
            0
        };
        loadTextureFromFile(scene, texture);
        scene.textureList.emplace_back(texture);
    }
    for (auto meshInfo : meshInfoList) {
        std::string path;
        // TODO: need to fix mesh initialization here
        Mesh mesh;
        mesh.normalMapID = -1;
        meshInfo.at("name").get_to(mesh.name);
        meshInfo.at("path").get_to(path);
        auto diffuseMaps = meshInfo.at("diffuseTexture");
        auto specularMaps = meshInfo.at("specularTexture");

        // TODO: decouple mesh info with which shader to use to render them
        // TODO: hard-code shaderIdx for now
        //---- handling shader ----
        std::string shaderName;
        meshInfo.at("shader").get_to(shaderName);
        if (shaderName == "blinnPhong") {
            mesh.shaderIdx = 3;
        } else if (shaderName == "flat") {
            mesh.shaderIdx = 5;
        } else if (shaderName == "pbr") {
            mesh.shaderIdx = 6;
        }
        //-------------------------

        for (auto& diffuseMap : diffuseMaps) {
            std::string textureName;
            diffuseMap.get_to(textureName);
            if (textureName == "") {
                continue;
            }
            mesh.diffuseMapTable.insert(std::pair<std::string, int>(textureName, -1));
        }
        for (auto& specularMap : specularMaps) {
            std::string textureName;
            specularMap.get_to(textureName);
            if (textureName == "") {
                continue;
            }
            mesh.specularMapTable.insert(std::pair<std::string, int>(textureName, -1));
        }
        if (meshInfo.at("normalMapName") != "") {
            meshInfo.at("normalMapName").get_to(mesh.normalMapName);
        }
        if (meshInfo.at("aoMapName") != "") {
            meshInfo.at("aoMapName").get_to(mesh.aoMapName);
        } 
        if (meshInfo.at("roughnessMapName") != "") {
            meshInfo.at("roughnessMapName").get_to(mesh.roughnessMapName);
        } 
        // binding diffuse map ids
        for(auto itr = mesh.diffuseMapTable.begin(); itr != mesh.diffuseMapTable.end(); itr++) {
            int idx = findTextureIndex(scene, itr->first);
            if (idx != -1) {
                itr->second = idx;
            }
        }
        // binding specular map ids
        for(auto itr = mesh.specularMapTable.begin(); itr != mesh.specularMapTable.end(); itr++) {
            int idx = findTextureIndex(scene, itr->first);
            if (idx != -1) {
                itr->second = idx;
            }
        }

        loadMesh(mesh, path.c_str());
        findTexturesForMesh(scene, mesh);
        scene.meshList.emplace_back(mesh);
    }
    int instanceID = 0;
    for (auto instanceInfo : instanceInfoList) {
        std::string meshName;
        MeshInstance meshInstance = {};
        instanceInfo.at("meshName").get_to(meshName);
        auto xformInfo = instanceInfo.at("xform");
        Transform xform = instanceInfo.at("xform").get<Transform>();
        for (int i = 0; i < scene.meshList.size(); i++) {
            if (scene.meshList[i].name == meshName) {
                meshInstance.instanceID = instanceID;
                meshInstance.meshID = i;
                scene.instanceList.emplace_back(meshInstance);
                scene.xformList.emplace_back(xform);
                break;
            }
        }
        instanceID++;
    }
    if (sceneJson["hasSkybox"]) {
        std::cout << "the scene contains skybox" << std::endl;
        auto skyboxInfo = sceneJson["skybox"];
        skyboxInfo.at("name").get_to(scene.skybox.name);
        CubemapTexture cube;
        auto texturePaths = skyboxInfo["texturePaths"];
        
        // TODO: fix the enums
        texturePaths.at("front").get_to(cube.textures[CubemapTexture::front].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::front]);

        texturePaths.at("back").get_to(cube.textures[CubemapTexture::back].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::back]);

        texturePaths.at("left").get_to(cube.textures[CubemapTexture::left].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::left]);

        texturePaths.at("right").get_to(cube.textures[CubemapTexture::right].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::right]);

        texturePaths.at("top").get_to(cube.textures[CubemapTexture::top].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::top]);

        texturePaths.at("bottom").get_to(cube.textures[CubemapTexture::bottom].path);
        loadTextureFromFile(scene, cube.textures[CubemapTexture::bottom]);

        scene.cubemapTextures.emplace_back(cube);

        setupSkybox(scene.skybox, cube);
    }
}

int SceneManager::findTextureIndex(const Scene& scene, const std::string& textureName) {
    for (int i = 0; i < scene.textureList.size(); i++) {
            if (scene.textureList[i].name == textureName) {
                return i;
            }
    }
    return -1;
}

void SceneManager::findTexturesForMesh(Scene& scene, Mesh& mesh) {
    for (int i = 0; i < scene.textureList.size(); i++) {
        if (scene.textureList[i].name == mesh.normalMapName) {
            mesh.normalMapID = i;
        }
        if (scene.textureList[i].name == mesh.aoMapName) {
            mesh.aoMapID = i;
        }
        if (scene.textureList[i].name == mesh.roughnessMapName) {
            mesh.roughnessMapID = i;
        }
    }
}

void SceneManager::setupSkybox(Skybox& skybox, CubemapTexture& texture) {
    //---- Setup cube mesh ----
    glGenVertexArrays(1, &skybox.vao);
    glBindVertexArray(skybox.vao);
    glGenBuffers(1, &skybox.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, skybox.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    //---- Setup textures ----
    glGenTextures(1, &skybox.cubmapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.cubmapTexture);
    for (int f = 0; f < 6; f++) {
        int w, h, numChannels;
        stbi_set_flip_vertically_on_load(false);
        unsigned char* imageData = stbi_load(texture.textures[f].path.c_str(), &w, &h, &numChannels, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}