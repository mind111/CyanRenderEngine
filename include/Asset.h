#pragma once

#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#include "stb_image.h"
#include "json.hpp"
#include "tiny_gltf.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Common.h"
#include "Texture.h"
#include "Scene.h"
#include "Mesh.h"
#include "CyanAPI.h"

struct Asset
{

};

// TODO: improve readability

class AssetManager
{
public:

    struct LoadedNode
    {
        SceneNode* m_sceneNode;
        std::vector<u32> m_child;
    };

    Cyan::Texture* loadGltfTexture(tinygltf::Model& model, i32 index);
    // TODO: Normalize mesh scale
    void loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, 
                        SceneNode* parentSceneNode, tinygltf::Node& node, u32 numNodes);
    Cyan::Mesh* loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
    void loadGltfTextures(tinygltf::Model& model);
    void loadGltf(Scene* scene, const char* filename, Transform transform);

    void loadScene(Scene* scene, const char* file);
    void loadNodes(nlohmann::basic_json<std::map>& nodeInfoList);
    void loadEntities(Scene* scene, nlohmann::basic_json<std::map>& entityInfoList);
    void loadTextures(nlohmann::basic_json<std::map>& textureInfoList);
    void loadMeshes(Scene* scene, nlohmann::basic_json<std::map>& meshInfoList);

    tinygltf::TinyGLTF m_loader;
    Assimp::Importer m_importer;
    std::vector<SceneNode*> m_nodes;
};