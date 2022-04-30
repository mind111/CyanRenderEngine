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
#include "AssetFactory.h"

namespace Cyan
{
    // todo: make this a singleton
    class AssetManager
    {
    public:
        AssetManager();
        ~AssetManager() { };
        static AssetManager* get() { return singletonPtr; }

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
        static AssetManager* singletonPtr;

        // asset tables
        std::unordered_map<std::string, Mesh*> m_meshMap;
        std::unordered_map<std::string, BaseMaterial*> m_materialMap;

        template <typename T>
        Mesh::Submesh<T>* createSubmesh(const std::vector<typename T::Vertex>& vertices, const std::vector<u32>& indices)
        {
            Mesh::Submesh<T>* sm = new Submesh(vertices, indices);
            return sm;
        }

        /*
        * create geometry data first, and then pass in to create a mesh
        */
        Mesh* createMesh(const char* name, const std::vector<BaseSubmesh*>& submeshes)
        {
            Mesh* mesh = new Mesh(name, submeshes);
            // register mesh object into the asset table
            m_meshMap.insert(mesh->name, mesh);
            return mesh;
        }

        template <typename MaterialType>
        MaterialType* createMaterial(const char* name)
        {
            MaterialType* material = new MaterialType(name);
            m_materialMap.insert(std::string(name), material);
            return material;
        }

        // getters
        template <typename T>
        Mesh* getAsset(const char* meshName) 
        { 
            const auto& entry = m_meshMap.find(std::string(meshName));
            if (entry == m_meshMap.end())
            {
                return nullptr;
            }
            return entry->second;
        }

        template <typename T>
        Material<T>* getAsset(const char* assetName) 
        { 
            auto entry = m_materialMap.find(std::string(matlName));
            if (Material<T>::getAssetTypeIdentifier() == entry->second->getAssetTypeIdentifier())
            {
                return static_cast<Material<T>*>(entry->second);
            }
            else
            {
                CYAN_ASSERT(0, "Asset type error")
            }
        }
    };
}