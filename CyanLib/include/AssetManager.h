#pragma once

#include <iostream>
#include <string>
#include <mutex>
#include <unordered_map>

#include <stbi/stb_image.h>
#include <tiny_gltf/json.hpp>

#include "Singleton.h"
#include "Common.h"
#include "Texture.h"
#include "TextureAtlas.h"
#include "Mesh.h"
#include "Material.h"

#define ASSET_PATH "C:/dev/cyanRenderEngine/asset/"

namespace Cyan
{
    class AssetImporter;

    class AssetManager : public Singleton<AssetManager> 
    {
        friend class AssetImporter;
    public:
        struct DefaultTextures 
        {
            std::shared_ptr<Texture2D> checkerDark = nullptr;
            std::shared_ptr<Texture2D> checkerOrange = nullptr;
            std::shared_ptr<Texture2D> gridDark = nullptr;
            std::shared_ptr<Texture2D> gridOrange = nullptr;
            std::shared_ptr<Texture2D> blueNoise_128x128_R = nullptr;
            std::shared_ptr<Texture2D> blueNoise_1024x1024_RGBA = nullptr;
        } m_defaultTextures;

        struct DefaultShapes 
        {
            std::shared_ptr<StaticMesh> fullscreenQuad = nullptr;
            std::shared_ptr<StaticMesh> quad = nullptr;
            std::shared_ptr<StaticMesh> unitCubeMesh = nullptr;
            std::shared_ptr<StaticMesh> sphere = nullptr;
            std::shared_ptr<StaticMesh> icosphere = nullptr;
            std::shared_ptr<StaticMesh> boundingSphere = nullptr;
            std::shared_ptr<StaticMesh> disk = nullptr;
        } m_defaultShapes;

        AssetManager();
        ~AssetManager();

        void initialize();

        static std::shared_ptr<Image> createImage(const char* name);
        static std::shared_ptr<Texture2D> createTexture2D(const char* name, std::shared_ptr<Image> srcImage, const Sampler2D& inSampler = Sampler2D{});
        static std::shared_ptr<StaticMesh> createStaticMesh(const char* name, u32 numSubmeshes);
        static std::shared_ptr<Material> createMaterial(const char* name, const char* materialSourcePath, const Material::SetupDefaultInstance& setupDefaultInstance);
        template <typename T>
        static std::shared_ptr<T> findAsset(const char* name);

        std::unordered_map<std::string, PackedImageDesc> packedImageMap;
        std::unordered_map<std::string, SubtextureDesc> packedTextureMap;

        PackedImageDesc packImage(Image* inImage)
        {
            const auto& entry = packedImageMap.find(inImage->m_name.c_str());
            if (entry == packedImageMap.end())
            {
                PackedImageDesc outDesc = { -1, -1 };
                GfxTexture::Format format;
                switch (inImage->m_bitsPerChannel)
                {
                case 8:
                    switch (inImage->m_numChannels)
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
                packedImageMap.insert({ inImage->m_name, outDesc });
            }
            return packedImageMap[inImage->m_name];
        }

        PackedImageDesc getPackedImageDesc(const char* m_name)
        {
            auto entry = packedImageMap.find(m_name);
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

        SubtextureDesc getPackedTextureDesc(const char* m_name)
        {
            auto entry = packedTextureMap.find(m_name);
            if (entry != packedTextureMap.end())
            {
                return entry->second;
            }
            return { -1, -1 };
        }

        std::array<Texture2DAtlas*, (u32)Texture2DAtlas::Format::kCount> atlases = { nullptr };
    private:
        // why not just storing the mapping as <string, void*> ?
        std::unordered_map<std::string, std::shared_ptr<StaticMesh>> m_meshMap;
        std::unordered_map<std::string, std::shared_ptr<Image>> m_imageMap;
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_textureMap;
        std::unordered_map<std::string, std::shared_ptr<Material>> m_newMaterialMap;

        std::unique_ptr<AssetImporter> m_assetImporter = nullptr;
    };
}