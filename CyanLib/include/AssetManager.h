#pragma once

#include <iostream>
#include <string>
#include <map>
#include <algorithm>

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
            Mesh* fullscreenQuad = nullptr;
            Mesh* quad = nullptr;
            Mesh* unitCubeMesh = nullptr;
            Mesh* sphere = nullptr;
            Mesh* icosphere = nullptr;
            Mesh* boundingSphere = nullptr;
            Mesh* disk = nullptr;
        } m_defaultShapes;

        AssetManager();
        ~AssetManager() { };

        void initialize();
        void importGltfNode(Scene* scene, tinygltf::Model& model, Entity* parent, tinygltf::Node& node);
        Mesh* importGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        Cyan::Texture2D* importGltfTexture(tinygltf::Model& model, tinygltf::Texture& gltfTexture);
        void importGltfTextures(tinygltf::Model& model);
        static void importGltf(Scene* scene, const char* filename, const char* name=nullptr);
        static void importGltfAsync(Scene* scene, const char* filename);
        static void importGlb(Scene* scene, const char* filename);
        std::vector<ISubmesh*> importObj(const char* baseDir, const char* filename);

        /**
        * Creating a texture from scratch; `name` must be unique
        */
        static Texture2D* createTexture2D(const char* name, const Texture2D::Spec& spec, const Sampler2D& inSampler = Sampler2D{});
        static Texture2D* createTexture2D(const char* name, Image* srcImage, bool bGenerateMipmap, const Sampler2D& inSampler = Sampler2D{});
#if 0
        static Texture3D* createTexture3D(const char* name, const Texture3D::Spec& spec, Texture::Parameter parameter=ITexture::Parameter{ }) {
            Texture3D* outTexture = getAsset<Texture3D>(name);
            if (!outTexture)
            {
                Texture3D* outTexture = new Texture3D(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }
#endif
        static TextureCube* createTextureCube(const char* name, const TextureCube::Spec& spec, const SamplerCube& inSampler = SamplerCube{ });
        static DepthTexture2D* createDepthTexture(const char* name, u32 width, u32 height);
        static Image* importImage(const char* name, const char* filename);
        static Image* importImage(const char* name, u8* mem, u32 sizeInBytes);
        static Texture2D* importTexture2D(const char* textureName, const char* srcImageFile, bool bGenerateMipmap, const Sampler2D& inSampler);
        static Texture2D* importTexture2D(const char* textureName, const char* srcImageName, const char* srcImageFile, bool bGenerateMipmap, const Sampler2D& inSampler);
        static Material& createMaterial(const char* name);
        static MaterialTextureAtlas& createPackedMaterial(const char* name);

        // getters
        template <typename T>
        static T* getAsset(const char* assetName);

        template <>
        static Mesh* getAsset<Mesh>(const char* meshName) { 
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
        static DepthTexture2D* getAsset<DepthTexture2D>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<DepthTexture2D*>(entry->second);
        }

#if 0
        template<>
        static Texture3D* getAsset<Texture3D>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<Texture3D*>(entry->second);
        }
#endif
        
        template<>
        static TextureCube* getAsset<TextureCube>(const char* textureName)
        {
            const auto& entry = singleton->m_textureMap.find(textureName);
            if (entry == singleton->m_textureMap.end())
            {
                return nullptr;
            }
            return dynamic_cast<TextureCube*>(entry->second);
        }

        template <>
        static Material* getAsset<Material>(const char* name) {
            auto entry = singleton->m_materialMap.find(std::string(name));
            if (entry != singleton->m_materialMap.end()) {
                return &entry->second;
            }
            return nullptr;
        }

        static const std::vector<Texture*>& getTextures()
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
        * create an empty mesh assuming that it's geometry data will be filled in later
        */
        static Mesh* createMesh(const char* name)
        {
            Mesh* parent = new Mesh(name);
            // register mesh object into the asset table
            singleton->m_meshMap.insert({ parent->name, parent });
            return parent;
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

        std::unordered_map<std::string, PackedImageDesc> packedImageMap;
        std::unordered_map<std::string, PackedTextureDesc> packedTextureMap;

        PackedImageDesc packImage(const Image& inImage)
        {
            const auto& entry = packedImageMap.find(inImage.name.c_str());
            if (entry == packedImageMap.end())
            {
                PackedImageDesc outDesc = { -1, -1 };
                Texture::Format format;
                switch (inImage.bitsPerChannel)
                {
                case 8:
                    switch (inImage.numChannels)
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
                packedImageMap.insert({inImage.name, outDesc });
            }
            return packedImageMap[inImage.name];
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

        PackedTextureDesc getPackedTextureDesc(const char* name)
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
        void addTexture(Texture* inTexture) 
        {
            singleton->m_textureMap.insert({ inTexture->name, inTexture });
            singleton->m_textures.push_back(inTexture);
        }

        void* m_objLoader;
        void* m_gltfLoader;
        tinygltf::TinyGLTF m_gltfImporter;

        std::vector<Mesh*> m_meshes;
        std::vector<Image*> m_images;
        std::vector<Texture*> m_textures;

        // todo: need to switch to use indices at some point
        std::unordered_map<std::string, std::unique_ptr<Scene>> m_sceneMap;
        std::unordered_map<std::string, Mesh*> m_meshMap;
        std::unordered_map<std::string, Image*> m_imageMap;
        std::unordered_map<std::string, Texture*> m_textureMap;
        std::unordered_map<std::string, Material> m_materialMap;
        std::unordered_map<std::string, MaterialTextureAtlas> m_packedMaterialMap;
    };
}