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
    // todo: differentiate import...() from load...(), import refers to importing raw scene data, load refers to loading serialized binary
    class AssetManager
    {
    public:
        AssetManager();
        ~AssetManager() { };
        static AssetManager* get() { return singleton; }

        struct LoadedNode
        {
            SceneNode* m_sceneNode;
            std::vector<u32> m_child;
        };
        Cyan::Texture* loadGltfTexture(const char* nodeName, tinygltf::Model& model, i32 index);
        SceneNode* loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, SceneNode* parentSceneNode, tinygltf::Node& node, u32 numNodes);
        Mesh* loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        void loadGltfTextures(const char* nodeName, tinygltf::Model& model);
        SceneNode* loadGltf(Scene* scene, const char* filename, const char* name, Transform transform);
        std::vector<BaseSubmesh*> loadObj(const char* baseDir, const char* filename, bool generateLightMapUv);
        Mesh* loadMesh(std::string& path, const char* name, bool normalize, bool generateLightMapUv);
        void importScene(Scene* scene, const char* file);
        void importSceneNodes(Scene* scene, nlohmann::basic_json<std::map>& nodeInfoList);
        void importEntities(Scene* scene, nlohmann::basic_json<std::map>& entityInfoList);
        void importTextures(nlohmann::basic_json<std::map>& textureInfoList);
        void importMeshes(Scene* scene, nlohmann::basic_json<std::map>& meshInfoList);

        void* m_objLoader;
        void* m_gltfLoader;
        tinygltf::TinyGLTF m_loader;
        std::vector<SceneNode*> m_nodes;
        static AssetManager* singleton;

        // asset tables
        std::unordered_map<std::string, Texture*> m_textureMap;
        std::unordered_map<std::string, Mesh*> m_meshMap;
        // material instances
        std::unordered_map<std::string, IMaterial*> m_materialMap;

        template <typename T>
        Mesh::Submesh<T>* createSubmesh(const std::vector<typename T::Vertex>& vertices, const std::vector<u32>& indices)
        {
            Mesh::Submesh<T>* sm = new Mesh::Submesh<T>(vertices, indices);
            return sm;
        }

        /*
        * create geometry data first, and then pass in to create a mesh
        */
        Mesh* createMesh(const char* name, const std::vector<BaseSubmesh*>& submeshes)
        {
            Mesh* parent = new Mesh(name, submeshes);
            // register mesh object into the asset table
            m_meshMap.insert({ parent->name, parent });
            return parent;
        }

        template <typename MaterialType>
        MaterialType* createMaterial(const char* name)
        {
            MaterialType* material = new MaterialType(name);
            m_materialMap.insert({ std::string(name), material });
            return material;
        }

        // getters
        template <typename T>
        static T* getAsset(const char* assetName);

        template <>
        static Mesh* getAsset<Mesh>(const char* meshName) 
        { 
            const auto& entry = singleton->m_meshMap.find(std::string(meshName));
            if (entry == singleton->m_meshMap.end())
            {
                return nullptr;
            }
            return entry->second;
        }

        // todo: this maybe problematic
        template <template <typename> typename M, typename T>
        static M<T>* getAsset(const char* matlName) 
        { 
            auto entry = singleton->m_materialMap.find(std::string(matlName));
            if (M<T>::getAssetClassTypeDesc() != entry->second->getAssetObjectTypeDesc())
            {
                return nullptr;
            }
            return static_cast<M<T>*>(entry->second);
        }
    };
}