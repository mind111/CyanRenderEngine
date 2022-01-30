#pragma once

#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#include "stb_image.h"
#include "json.hpp"
#include "tiny_gltf.h"

#include "Common.h"
#include "Texture.h"
#include "Scene.h"
#include "Mesh.h"
#include "CyanAPI.h"

namespace Cyan
{
    class AssetManager
    {
    public:
        struct LoadedNode
        {
            SceneNode* m_sceneNode;
            std::vector<u32> m_child;
        };
        Cyan::Texture* loadGltfTexture(const char* nodeName, tinygltf::Model& model, i32 index);
        // TODO: fix normalizing mesh scale
        SceneNode*     loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, SceneNode* parentSceneNode, tinygltf::Node& node, u32 numNodes);
        Cyan::Mesh*    loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        void           loadGltfTextures(const char* nodeName, tinygltf::Model& model);
        SceneNode*     loadGltf(Scene* scene, const char* filename, const char* name, Transform transform);
        Mesh*          loadObj(const char* baseDir, const char* filename, bool generateLightMapUv);
        Mesh*          loadMesh(std::string& path, const char* name, bool normalize, bool generateLightMapUv);
        void           loadScene(Scene* scene, const char* file);
        void           loadNodes(Scene* scene, nlohmann::basic_json<std::map>& nodeInfoList);
        void           loadEntities(Scene* scene, nlohmann::basic_json<std::map>& entityInfoList);
        void           loadTextures(nlohmann::basic_json<std::map>& textureInfoList);
        void           loadMeshes(Scene* scene, nlohmann::basic_json<std::map>& meshInfoList);

        void* m_objLoader;
        void* m_gltfLoader;
        tinygltf::TinyGLTF m_loader;
        std::vector<SceneNode*> m_nodes;
    };
}