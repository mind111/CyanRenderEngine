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
    // todo: differentiate import...() from load...(), import refers to importing raw scene data, load refers to loading serialized binary
    class AssetManager
    {
    public:
        AssetManager();
        ~AssetManager() { };

        void initialize()
        {
            m_defaultMaterial = createMaterial<PBRMatl>("DefaultMaterial");
        }

        static AssetManager* get() { return singleton; }

        struct LoadedNode
        {
            SceneComponent* m_sceneNode;
            std::vector<u32> m_child;
        };

        Cyan::Texture2DRenderable* loadGltfTexture(const char* nodeName, tinygltf::Model& model, i32 index);
        SceneComponent* loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, SceneComponent* parentSceneNode, tinygltf::Node& node, u32 numNodes);
        Mesh* loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        void loadGltfTextures(const char* nodeName, tinygltf::Model& model);
        SceneComponent* loadGltf(Scene* scene, const char* filename, const char* name, Transform transform);
        std::vector<ISubmesh*> loadObj(const char* baseDir, const char* filename, bool generateLightMapUv);
        Mesh* loadMesh(std::string& path, const char* name, bool normalize, bool generateLightMapUv);
        void importScene(Scene* scene, const char* file);
        void importSceneNodes(Scene* scene, nlohmann::basic_json<std::map>& nodeInfoList);
        void importEntities(Scene* scene, nlohmann::basic_json<std::map>& entityInfoList);
        void importTextures(nlohmann::basic_json<std::map>& textureInfoList);
        void importMeshes(Scene* scene, nlohmann::basic_json<std::map>& meshInfoList);

        template <typename T>
        Mesh::Submesh<T>* createSubmesh(const std::vector<typename T::Vertex>& vertices, const std::vector<u32>& indices)
        {
            Mesh::Submesh<T>* sm = new Mesh::Submesh<T>(vertices, indices);
            return sm;
        }

        /*
        * create geometry data first, and then pass in to create a mesh
        */
        Mesh* createMesh(const char* name, const std::vector<ISubmesh*>& submeshes)
        {
            Mesh* parent = new Mesh(name, submeshes);
            // register mesh object into the asset table
            m_meshMap.insert({ parent->name, parent });
            return parent;
        }

        /**
        * Creating a texture from scratch; `name` must be unique
        */
        static Texture2DRenderable* createTexture2D(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            Texture2DRenderable* texture = new Texture2DRenderable(name, spec, parameter);
            return texture;
        }

        static Texture3DRenderable* createTexture3D(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            Texture3DRenderable* texture = new Texture3DRenderable(name, spec, parameter);
            return texture;
        }

        static TextureCubeRenderable* createTextureCube(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            TextureCubeRenderable* texture = new TextureCubeRenderable(name, spec, parameter);
            return texture;
        }

        static DepthTexture* createDepthTexture(const char* name, u32 width, u32 height)
        {
            DepthTexture* texture = getAsset<DepthTexture>(name);
            if (!texture)
            {
                texture = new DepthTexture(name, width, height);
            }
            return texture;
        }

        /**
        * Importing texture from an image file
        */
        static Texture2DRenderable* importTexture2D(const char* name, const char* imageFilePath, ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            return nullptr;
        }

        template <typename MaterialType>
        MaterialType* createMaterial(const char* name)
        {
            MaterialType* material = new MaterialType(name);
            m_materialMap.insert({ std::string(name), material });
            return material;
        }

        // getters
        static IMaterial* getDefaultMaterial()
        {
            return singleton->m_defaultMaterial;
        }

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

        template<>
        static Texture2DRenderable* getAsset<Texture2DRenderable>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<Texture2DRenderable*>(entry->second);
        }

        template<>
        static DepthTexture* getAsset<DepthTexture>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<DepthTexture*>(entry->second);
        }

        template<>
        static Texture3DRenderable* getAsset<Texture3DRenderable>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<Texture3DRenderable*>(entry->second);
        }
        
        template<>
        static TextureCubeRenderable* getAsset<TextureCubeRenderable>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<TextureCubeRenderable*>(entry->second);
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

    private:
        void* m_objLoader;
        void* m_gltfLoader;
        tinygltf::TinyGLTF m_loader;
        std::vector<SceneComponent*> m_nodes;
        static AssetManager* singleton;

        // asset tables
        std::unordered_map<std::string, std::unique_ptr<Scene>> m_sceneMap;
        std::unordered_map<const char*, ITextureRenderable*> m_textureMap;
        std::unordered_map<std::string, Mesh*> m_meshMap;

        // material instances
        PBRMatl* m_defaultMaterial = nullptr;
        std::unordered_map<std::string, IMaterial*> m_materialMap;
    };
}