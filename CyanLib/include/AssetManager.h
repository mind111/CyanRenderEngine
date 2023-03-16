#pragma once


#include <iostream>
#include <string>
#include <map>
#include <algorithm>
#include <mutex>
#include <thread>

#include <stbi/stb_image.h>
#include <tiny_gltf/json.hpp>
#include <tiny_gltf/tiny_gltf.h>

#include "Singleton.h"
#include "Common.h"
#include "Texture.h"
#include "Scene.h"
#include "Mesh.h"
#include "Material.h"
#include "CyanAPI.h"

#define ASSET_PATH "C:/dev/cyanRenderEngine/asset/"

namespace Cyan
{
    class AssetManager : public Singleton<AssetManager> 
    {
        friend class AssetImporter;
    public:
        struct DefaultTextures 
        {
            Texture2D* checkerDark = nullptr;
            Texture2D* checkerOrange = nullptr;
            Texture2D* gridDark = nullptr;
            Texture2D* gridOrange = nullptr;
            Texture2D* blueNoise_1024x1024 = nullptr;
        } m_defaultTextures;

        struct DefaultShapes 
        {
            StaticMesh* fullscreenQuad = nullptr;
            StaticMesh* quad = nullptr;
            StaticMesh* unitCubeMesh = nullptr;
            StaticMesh* sphere = nullptr;
            StaticMesh* icosphere = nullptr;
            StaticMesh* boundingSphere = nullptr;
            StaticMesh* disk = nullptr;
        } m_defaultShapes;

        AssetManager();
        ~AssetManager() { };

        void initialize();
        void update();
        static void import(Scene* scene, const char* filename);

        using AssetInitFunc = std::function<void(Asset* asset)>;
        std::mutex deferredInitMutex;
        struct DeferredInitTask
        {
            Asset* asset = nullptr;
            AssetInitFunc initFunc;
        };
        std::queue<DeferredInitTask> m_deferredInitQueue;
        static void deferredInitAsset(Asset* asset, const AssetInitFunc& inFunc);

        void importGltfNode(Scene* scene, tinygltf::Model& model, Entity* parent, tinygltf::Node& node);
        StaticMesh* importGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        Cyan::GfxTexture2D* importGltfTexture(tinygltf::Model& model, tinygltf::Texture& gltfTexture);
        void importGltfTextures(tinygltf::Model& model);
        static void importGltf(Scene* scene, const char* filename, const char* name=nullptr);
        static void importGltfAsync(Scene* scene, const char* filename);
        static void importGlb(Scene* scene, const char* filename);

        // textures
        static Image* createImage(const char* name);
        static Texture2D* createTexture2D(const char* name, Image* srcImage, const Sampler2D& inSampler = Sampler2D{});
        // static Texture2DBindless* createTexture2DBindless(const char* name, Image* srcImage, const Sampler2D& inSampler = Sampler2D{});

        // meshes
        static StaticMesh* importWavefrontObj(const char* meshName, const char* baseDir, const char* filename);
        static StaticMesh* createStaticMesh(const char* name);
        static StaticMesh* createStaticMesh(const char* name, u32 numSubmeshes);

        static Material* createMaterial(const char* name);
        static i32 getMaterialIndex(Material* material);
        // static MaterialBindless* createMaterialBindless(const char* name);
        // static MaterialTextureAtlas* createPackedMaterial(const char* name);

        // getters
        template <typename T>
        static T* getAsset(const char* assetName);

        template <>
        static StaticMesh* getAsset<StaticMesh>(const char* meshName) { 
            const auto& entry = singleton->m_meshMap.find(std::string(meshName));
            if (entry == singleton->m_meshMap.end())
            {
                return nullptr;
            }
            return entry->second;
        }

        template <>
        static Image* getAsset<Image>(const char* imageName) { 
            const auto& entry = singleton->m_imageMap.find(std::string(imageName));
            if (entry == singleton->m_imageMap.end())
            {
                return nullptr;
            }
            return entry->second;
        }

        template<>
        static Texture2D* getAsset<Texture2D>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<Texture2D*>(entry->second);
        }

        template<>
        static GfxTextureCube* getAsset<GfxTextureCube>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<GfxTextureCube*>(entry->second);
        }

        template <>
        static Material* getAsset<Material>(const char* name) 
        {
            auto entry = singleton->m_materialMap.find(std::string(name));
            if (entry != singleton->m_materialMap.end()) 
            {
                return singleton->m_materials[entry->second];
            }
            return nullptr;
        }

        static const std::vector<Texture2DBase*>& getTextures()
        {
            return singleton->m_textures;
        }

        std::unordered_map<std::string, PackedImageDesc> packedImageMap;
        std::unordered_map<std::string, SubtextureDesc> packedTextureMap;

        PackedImageDesc packImage(Image* inImage)
        {
            const auto& entry = packedImageMap.find(inImage->name.c_str());
            if (entry == packedImageMap.end())
            {
                PackedImageDesc outDesc = { -1, -1 };
                GfxTexture::Format format;
                switch (inImage->bitsPerChannel)
                {
                case 8:
                    switch (inImage->numChannels)
                    {
                    case 1:
                        format = PF_R8; 
                        outDesc.atlasIndex = (i32)Texture2DAtlas::Format::kR8;
                        outDesc.subimageIndex = atlases[(u32)Texture2DAtlas::Format::kR8]->packImage(inImage);
                        break;
                    case 3:
                        format = PF_RGB8; 
                        outDesc.atlasIndex = (i32)Texture2DAtlas::Format::kRGB8;
                        outDesc.subimageIndex = atlases[(u32)Texture2DAtlas::Format::kRGB8]->packImage(inImage);
                        break;
                    case 4: 
                        format = PF_RGBA8; 
                        outDesc.atlasIndex = (i32)Texture2DAtlas::Format::kRGBA8;
                        outDesc.subimageIndex = atlases[(u32)Texture2DAtlas::Format::kRGBA8]->packImage(inImage);
                        break;
                    default: assert(0); break;
                    }
                    break;
                case 16: 
                case 32: 
                default: 
                    assert(0); 
                    break;
                }
                packedImageMap.insert({ inImage->name, outDesc });
            }
            return packedImageMap[inImage->name];
        }

        PackedImageDesc getPackedImageDesc(const char* name)
        {
            auto entry = packedImageMap.find(name);
            if (entry != packedImageMap.end())
            {
                return entry->second;
            }
            return { -1, -1 };
        }

        void addPackedTexture(const char* packedTextureName, const PackedImageDesc& packedImageDesc, bool bGenerateMipmap, Sampler2D& inSampler)
        {
            assert(bGenerateMipmap);

            auto entry = packedTextureMap.find(packedTextureName);
            if (entry == packedTextureMap.end())
            {
                i32 subtextureIndex = atlases[packedImageDesc.atlasIndex]->addSubtexture(packedImageDesc.subimageIndex, inSampler, bGenerateMipmap);
                if (subtextureIndex >= 0)
                {
                    packedTextureMap.insert({ packedTextureName, { packedImageDesc.atlasIndex, subtextureIndex } });
                }
                else
                {
                    assert(0);
                }
            }
        }

        SubtextureDesc getPackedTextureDesc(const char* name)
        {
            auto entry = packedTextureMap.find(name);
            if (entry != packedTextureMap.end())
            {
                return entry->second;
            }
            return { -1, -1 };
        }

        std::array<Texture2DAtlas*, (u32)Texture2DAtlas::Format::kCount> atlases = { nullptr };
    private:
        /**
        * Adding a texture into the asset data base
        */
        void addTexture(Texture2DBase* inTexture) 
        {
            singleton->m_textureMap.insert({ inTexture->name, inTexture });
            singleton->m_textures.push_back(inTexture);
        }

        void* m_objLoader;
        void* m_gltfLoader;
        tinygltf::TinyGLTF m_gltfImporter;

        std::vector<StaticMesh*> m_meshes;
        std::vector<Image*> m_images;
        std::vector<Texture2DBase*> m_textures;
        std::vector<Material*> m_materials;

        // todo: need to switch to use indices at some point
        std::unordered_map<std::string, std::unique_ptr<Scene>> m_sceneMap;
        std::unordered_map<std::string, StaticMesh*> m_meshMap;
        std::unordered_map<std::string, Image*> m_imageMap;
        std::unordered_map<std::string, Texture2DBase*> m_textureMap;
        std::unordered_map<std::string, u32> m_materialMap;
        // std::unordered_map<std::string, MaterialTextureAtlas> m_packedMaterialMap;
    };
}