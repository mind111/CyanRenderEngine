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
#include "Material.h"
#include "CyanAPI.h"

namespace Cyan
{
    // todo: differentiate import...() from load...(), import refers to importing raw scene data, load refers to loading serialized binary
    class AssetManager
    {
    public:

        struct DefaultTextures
        {
            Texture2DRenderable* checkerDark = nullptr;
            Texture2DRenderable* checkerOrange = nullptr;
            Texture2DRenderable* gridDark = nullptr;
            Texture2DRenderable* gridOrange = nullptr;
        } m_defaultTextures;

        AssetManager();
        ~AssetManager() { };

        void initialize()
        {
            /**
                initialize default geometries and shapes
            */ 
            // cube
            // sphere
            // cylinder

            /**
                initialize default textures
            */ 
#define DEFAULT_TEXTURE_FOLDER "../../asset/textures/defaults/"
            ITextureRenderable::Spec spec = { };
            spec.numMips = 1u;
            ITextureRenderable::Parameter parameter = { };
            parameter.wrap_r = ITextureRenderable::Parameter::WrapMode::WRAP;
            parameter.wrap_s = ITextureRenderable::Parameter::WrapMode::WRAP;
            parameter.wrap_t = ITextureRenderable::Parameter::WrapMode::WRAP;
            m_defaultTextures.checkerDark = importTexture2D("default_checker_dark", DEFAULT_TEXTURE_FOLDER "checker_dark.png", spec, parameter);
            m_defaultTextures.checkerOrange = importTexture2D("default_checker_orange", DEFAULT_TEXTURE_FOLDER "checker_orange.png", spec, parameter);
            m_defaultTextures.gridDark = importTexture2D("default_grid_dark", DEFAULT_TEXTURE_FOLDER "grid_dark.png", spec, parameter);
            m_defaultTextures.gridOrange = importTexture2D("default_grid_orange", DEFAULT_TEXTURE_FOLDER "grid_orange.png", spec, parameter);
#undef DEFAULT_TEXTURE_FOLDER

            /**
                initialize the default material 
            */ 
            createMaterial<PBRMaterial>("DefaultMaterial");
        }

        static AssetManager* get() { return singleton; }

#if 0
        struct LoadedNode
        {
            SceneComponent* m_sceneNode;
            std::vector<u32> m_child;
        };
#endif

        void importGltfNode(Scene* scene, tinygltf::Model& model, Entity* parent, tinygltf::Node& node);
        Mesh* importGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        Cyan::Texture2DRenderable* importGltfTexture(const char* nodeName, tinygltf::Model& model, i32 index);
        void importGltfTextures(const char* nodeName, tinygltf::Model& model);
        static void importGltf(Scene* scene, const char* filename, const char* name=nullptr);
        std::vector<ISubmesh*> importObj(const char* baseDir, const char* filename, bool generateLightMapUv);
        void importScene(Scene* scene, const char* file);
        void importEntities(Scene* scene, const nlohmann::basic_json<std::map>& entityInfoList);
        void importTextures(const nlohmann::basic_json<std::map>& textureInfoList);
        Mesh* importMesh(Scene* scene, std::string& path, const char* name, bool normalize, bool generateLightMapUv);
        void importMeshes(Scene* scene, const nlohmann::basic_json<std::map>& meshInfoList);

        /**
        * Creating a texture from scratch; `name` must be unique
        */
        static Texture2DRenderable* createTexture2D(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            Texture2DRenderable* outTexture = getAsset<Texture2DRenderable>(name);
            if (!outTexture)
            {
                outTexture = new Texture2DRenderable(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        static Texture3DRenderable* createTexture3D(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            Texture3DRenderable* outTexture = getAsset<Texture3DRenderable>(name);
            if (!outTexture)
            {
                Texture3DRenderable* outTexture = new Texture3DRenderable(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        static TextureCubeRenderable* createTextureCube(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            TextureCubeRenderable* outTexture = getAsset<TextureCubeRenderable>(name);
            if (!outTexture)
            {
                TextureCubeRenderable* outTexture = new TextureCubeRenderable(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        static DepthTexture* createDepthTexture(const char* name, u32 width, u32 height)
        {
            DepthTexture* outTexture = getAsset<DepthTexture>(name);
            if (!outTexture)
            {
                outTexture = new DepthTexture(name, width, height);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        /**
        * Importing texture from an image file
        */
        static Texture2DRenderable* importTexture2D(const char* name, const char* imageFilePath, ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            int width, height, numChannels;
            stbi_set_flip_vertically_on_load(1);
            // todo: do I need to distinguish between ldr and hdr pixel format format and use stbi_load/stbi_loadf accordingly?
            spec.pixelData = reinterpret_cast<u8*>(stbi_load(imageFilePath, &width, &height, &numChannels, 0));
            // todo: pixel format is hard coded for now
            if (numChannels == 3)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R8G8B8;
            }
            else if (numChannels == 4)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R8G8B8A8;
            }
            spec.width = width;
            spec.height = height;
            return createTexture2D(name, spec, parameter);
        }

        template <typename MaterialType>
        static MaterialType* createMaterial(const char* name)
        {
            MaterialType* material = new MaterialType(name);
            singleton->m_materialMap.insert({ std::string(name), material });
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

        template <>
        static IMaterial* getAsset<IMaterial>(const char* matlName)
        {
            auto entry = singleton->m_materialMap.find(std::string(matlName));
            if (entry != singleton->m_materialMap.end())
            {
                return entry->second;
            }
            return nullptr;
        }

        static const std::vector<ITextureRenderable*>& getTextures()
        {
            return singleton->m_textures;
        }

        template <typename T>
        static Mesh::Submesh<T>* createSubmesh(const std::vector<typename T::Vertex>& vertices, const std::vector<u32>& indices)
        {
            Mesh::Submesh<T>* sm = new Mesh::Submesh<T>(vertices, indices);
            return sm;
        }

        /*
        * create geometry data first, and then pass in to create a mesh
        */
        static Mesh* createMesh(const char* name, const std::vector<ISubmesh*>& submeshes)
        {
            Mesh* parent = new Mesh(name, submeshes);
            // register mesh object into the asset table
            singleton->m_meshMap.insert({ parent->name, parent });
            return parent;
        }

    private:

        /**
        * Adding a texture into the asset data base
        */
        void addTexture(ITextureRenderable* inTexture)
        {
            singleton->m_textureMap.insert({ inTexture->name, inTexture });
            singleton->m_textures.push_back(inTexture);
        }

        void* m_objLoader;
        void* m_gltfLoader;
        tinygltf::TinyGLTF m_gltfImporter;
        static AssetManager* singleton;

        // asset arrays for efficient iterating
        std::vector<Mesh*> m_meshes;
        std::vector<ITextureRenderable*> m_textures;

        // asset tables for efficient lookup
        std::unordered_map<std::string, std::unique_ptr<Scene>> m_sceneMap;
        std::unordered_map<std::string, ITextureRenderable*> m_textureMap;
        std::unordered_map<std::string, Mesh*> m_meshMap;

        // material instances
        std::unordered_map<std::string, IMaterial*> m_materialMap;
    };
}