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
    // todo: differentiate import...() from load...(), import refers to importing raw scene data, load refers to loading serialized binary
    class AssetManager : public Singleton<AssetManager> 
    {
    public:
        struct DefaultTextures 
        {
            Texture2D* checkerDark = nullptr;
            Texture2D* checkerOrange = nullptr;
            Texture2D* gridDark = nullptr;
            Texture2D* gridOrange = nullptr;
            Texture2D* blueNoise_16x16 = nullptr;
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

        void initialize() {
            /**
                initialize default geometries and shapes
            */ 
            // line

            // shared global quad mesh
            {
                float quadVerts[24] = {
                    -1.f, -1.f, 0.f, 0.f,
                     1.f,  1.f, 1.f, 1.f,
                    -1.f,  1.f, 0.f, 1.f,

                    -1.f, -1.f, 0.f, 0.f,
                     1.f, -1.f, 1.f, 0.f,
                     1.f,  1.f, 1.f, 1.f
                };

                std::vector<ISubmesh*> submeshes;
                std::vector<Triangles::Vertex> vertices(6);
                vertices[0].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[0].texCoord0 = glm::vec2(0.f, 0.f);
                vertices[1].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[1].texCoord0 = glm::vec2(1.f, 1.f);
                vertices[2].pos = glm::vec3(-1.f,  1.f, 0.f); vertices[2].texCoord0 = glm::vec2(0.f, 1.f);
                vertices[3].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[3].texCoord0 = glm::vec2(0.f, 0.f);
                vertices[4].pos = glm::vec3( 1.f, -1.f, 0.f); vertices[4].texCoord0 = glm::vec2(1.f, 0.f);
                vertices[5].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[5].texCoord0 = glm::vec2(1.f, 1.f);
                std::vector<u32> indices({ 0, 1, 2, 3, 4, 5 });
                submeshes.push_back(createSubmesh<Triangles>(vertices, indices));
                m_defaultShapes.fullscreenQuad = createMesh("FullScreenQuadMesh", submeshes);
            }

            // cube
            u32 numVertices = sizeof(cubeVertices) / sizeof(glm::vec3);
            std::vector<ISubmesh*> submeshes;
            std::vector<Triangles::Vertex> vertices(numVertices);
            std::vector<u32> indices(numVertices);
            for (u32 v = 0; v < numVertices; ++v)
            {
                vertices[v].pos = glm::vec3(cubeVertices[v * 3 + 0], cubeVertices[v * 3 + 1], cubeVertices[v * 3 + 2]);
                indices[v] = v;
            }
            submeshes.push_back(createSubmesh<Triangles>(vertices, indices));
            m_defaultShapes.unitCubeMesh = createMesh("UnitCubeMesh", submeshes);
            // quad
            m_defaultShapes.quad = createMesh("Quad", importObj(ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/quad.obj"));
            // sphere
            m_defaultShapes.sphere = createMesh("Sphere", importObj(ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/sphere.obj"));
            // icosphere
            m_defaultShapes.icosphere = createMesh("IcoSphere", importObj(ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/icosphere.obj"));
            // bounding sphere
            // todo: line mesh doesn't work
            m_defaultShapes.boundingSphere = createMesh("BoundingSphere", importObj(ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/bounding_sphere.obj"));
            // cylinder
            // disk
            m_defaultShapes.disk = createMesh("Disk", importObj(ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/disk.obj"));

            /**
            *   initialize default textures
            */ 
            ITexture::Spec spec = { };
            spec.numMips = 11u;
            ITexture::Parameter parameter = { };
            parameter.minificationFilter = FM_TRILINEAR;
            parameter.wrap_r = ITexture::Parameter::WrapMode::WRAP;
            parameter.wrap_s = ITexture::Parameter::WrapMode::WRAP;
            parameter.wrap_t = ITexture::Parameter::WrapMode::WRAP;
            m_defaultTextures.checkerDark = importTexture2D("default_checker_dark", ASSET_PATH "textures/defaults/checker_dark.png", spec, parameter);
            m_defaultTextures.checkerOrange = importTexture2D("default_checker_orange", ASSET_PATH "textures/defaults/checker_orange.png", spec, parameter);
            m_defaultTextures.gridDark = importTexture2D("default_grid_dark", ASSET_PATH "textures/defaults/grid_dark.png", spec, parameter);
            m_defaultTextures.gridOrange = importTexture2D("default_grid_orange", ASSET_PATH "textures/defaults/grid_orange.png", spec, parameter);

            {
                ITexture::Spec spec = { };
                ITexture::Parameter parameter = { };
                parameter.minificationFilter = FM_POINT;
                parameter.magnificationFilter = FM_POINT;
                parameter.wrap_r = WM_WRAP;
                parameter.wrap_s = WM_WRAP;
                parameter.wrap_t = WM_WRAP;
                m_defaultTextures.blueNoise_16x16 = AssetManager::importTexture2D("BlueNoise_16x16", ASSET_PATH "textures/noise/LDR_LLL1_0.png", spec, parameter);
                m_defaultTextures.blueNoise_1024x1024 = AssetManager::importTexture2D("BlueNoise_1024x1024", ASSET_PATH "textures/noise/LDR_RGBA_0.png", spec, parameter);
            }

#undef DEFAULT_TEXTURE_FOLDER

            /**
                initialize the default material 
            */ 
            createMaterial("DefaultMaterial");
        }

        void importGltfNode(Scene* scene, tinygltf::Model& model, Entity* parent, tinygltf::Node& node);
        Mesh* importGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        Cyan::Texture2D* importGltfTexture(tinygltf::Model& model, tinygltf::Texture& gltfTexture);
        void importGltfTextures(tinygltf::Model& model);
        static void importGltf(Scene* scene, const char* filename, const char* name=nullptr);

        // std::queue<Image> loadedImages;
        std::unordered_multimap<std::string, Texture2D*> imageDependencyMap;
        static void importGltfAsync(Scene* scene, const char* filename);
        static void importGltfEx(Scene* scene, const char* filename);
        std::vector<ISubmesh*> importObj(const char* baseDir, const char* filename);

        /**
        * Creating a texture from scratch; `name` must be unique
        */
        static Texture2D* createTexture2D(const char* name, const ITexture::Spec& spec, ITexture::Parameter parameter=ITexture::Parameter{ })
        {
            Texture2D* outTexture = getAsset<Texture2D>(name);
            if (!outTexture)
            {
                outTexture = new Texture2D(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

#if 0
        struct TextureLoadingTask
        {
            gltf::Glb* src = nullptr;
            gltf::Image* dstImage;
            Texture2D* dstTexture;

            void load()
            {
                // todo: check if dstImage is already loaded
                if (dstImage->bufferView >= 0)
                {
                    gltf::BufferView& bv = src->bufferViews[dstImage->bufferView];
                    u8* dataAddress = src->binaryChunk.data() + bv.byteOffset;
                    i32 hdr = stbi_is_hdr_from_memory(dataAddress, bv.byteLength);
                    if (hdr)
                    {
                        dstImage->bHdr = true;
                        dstImage->pixels = reinterpret_cast<u8*>(stbi_loadf_from_memory(dataAddress, bv.byteLength, &dstImage->width, &dstImage->height, &dstImage->numChannels, 0));
                    }
                    else
                    {
                        i32 is16Bit = stbi_is_16_bit_from_memory(dataAddress, bv.byteLength);
                        if (is16Bit)
                        {
                            dstImage->b16Bits = true;
                        }
                        else
                        {
                            dstImage->pixels = stbi_load_from_memory(dataAddress, bv.byteLength, &dstImage->width, &dstImage->height, &dstImage->numChannels, 0);
                        }
                    }
                }
            }
        };
#endif

        // todo: learn how to implement thread pool that automatically picks up work from the task queue
        static Texture2D* createTexture2DDeferred(const char* name)
        {
            Texture2D* outTexture = getAsset<Texture2D>(name);
            if (!outTexture)
            {
                // add a new texture loading task onto the task queue
            }
        }

        static Texture3D* createTexture3D(const char* name, const ITexture::Spec& spec, ITexture::Parameter parameter=ITexture::Parameter{ }) {
            Texture3D* outTexture = getAsset<Texture3D>(name);
            if (!outTexture)
            {
                Texture3D* outTexture = new Texture3D(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        static TextureCube* createTextureCube(const char* name, const ITexture::Spec& spec, ITexture::Parameter parameter=ITexture::Parameter{ })
        {
            TextureCube* outTexture = getAsset<TextureCube>(name);
            if (!outTexture)
            {
                outTexture = new TextureCube(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        static DepthTexture2D* createDepthTexture(const char* name, u32 width, u32 height)
        {
            DepthTexture2D* outTexture = getAsset<DepthTexture2D>(name);
            if (!outTexture)
            {
                outTexture = new DepthTexture2D(name, width, height);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        /**
        * Importing texture from an image file
        */
        static Texture2D* importTexture2D(const char* name, const char* filename, ITexture::Spec& spec, ITexture::Parameter parameter=ITexture::Parameter{ })
        {
            // todo: this is not a robust solutions to this!!! a better way maybe parse the file header to get the true file format
            // determine whether the given image is ldr or hdr based on file extension

            std::string path(filename);
            u32 found = path.find_last_of('.');
            std::string extension = path.substr(found, found + 1);

            int width, height, numChannels;
            stbi_set_flip_vertically_on_load(1);
            i32 isHdr = stbi_is_hdr(filename);
            if (isHdr)
            {
                spec.pixelData = reinterpret_cast<u8*>(stbi_loadf(filename, &width, &height, &numChannels, 0));
                if (numChannels == 3)
                {
                    spec.pixelFormat = ITexture::Spec::PixelFormat::RGB32F;
                }
                else if (numChannels == 4)
                {
                    spec.pixelFormat = ITexture::Spec::PixelFormat::RGBA32F;
                }
                spec.width = width;
                spec.height = height;
            }
            else
            {
                i32 is16Bits = stbi_is_16_bit(filename);
                if (is16Bits)
                {
                    spec.pixelData = reinterpret_cast<u8*>(stbi_load_16(filename, &width, &height, &numChannels, 0));
                    if (numChannels == 3)
                    {
                        spec.pixelFormat = ITexture::Spec::PixelFormat::RGB16F;
                    }
                    else if (numChannels == 4)
                    {
                        spec.pixelFormat = ITexture::Spec::PixelFormat::RGBA16F;
                    }
                    spec.width = width;
                    spec.height = height;
                }
                else
                {
                    spec.pixelData = reinterpret_cast<u8*>(stbi_load(filename, &width, &height, &numChannels, 0));
                    if (numChannels == 3)
                    {
                        spec.pixelFormat = ITexture::Spec::PixelFormat::RGB8;
                    }
                    else if (numChannels == 4)
                    {
                        spec.pixelFormat = ITexture::Spec::PixelFormat::RGBA8;
                    }
                    spec.width = width;
                    spec.height = height;
                }
            }

#if 0
            if (extension == ".hdr")
            {
                spec.pixelData = reinterpret_cast<u8*>(stbi_loadf(filename, &width, &height, &numChannels, 0));
                // todo: pixel format is hard coded for now
                if (numChannels == 3)
                {
                    spec.pixelFormat = ITexture::Spec::PixelFormat::RGB16F;
                }
                else if (numChannels == 4)
                {
                    spec.pixelFormat = ITexture::Spec::PixelFormat::RGBA16F;
                }
                spec.width = width;
                spec.height = height;
            }
            else
            {
                spec.pixelData = reinterpret_cast<u8*>(stbi_load(filename, &width, &height, &numChannels, 0));
                // todo: pixel format is hard coded for now
                if (numChannels == 3)
                {
                    spec.pixelFormat = ITexture::Spec::PixelFormat::RGB8;
                }
                else if (numChannels == 4)
                {
                    spec.pixelFormat = ITexture::Spec::PixelFormat::RGBA8;
                }
                spec.width = width;
                spec.height = height;
            }
#endif
            if (spec.pixelData)
            {
                return createTexture2D(name, spec, parameter);
            }
            return nullptr;
        }

        static Material& createMaterial(const char* name) 
        {
            std::string key = std::string(name);
            auto entry = singleton->m_materialMap.find(key);
            if (entry == singleton->m_materialMap.end()) 
            {
                Material matl = { };
                matl.name = std::string(name);
                singleton->m_materialMap.insert({ key, matl });
            }
            return singleton->m_materialMap[key];
        }

        static MaterialTextureAtlas& createPackedMaterial(const char* name)
        {
            auto entry = singleton->m_packedMaterialMap.find(name);
            if (entry == singleton->m_packedMaterialMap.end()) 
            {
                MaterialTextureAtlas matl = { };
                matl.name = std::string(name);
                singleton->m_packedMaterialMap.insert({ matl.name, matl});
            }
            return singleton->m_packedMaterialMap[name];
        }

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

        static const std::vector<ITexture*>& getTextures()
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
                ITexture::Spec::PixelFormat format;
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

        void addPackedTexture(const char* packedTextureName, const PackedImageDesc& packedImageDesc, ITexture::Parameter& params)
        {
            auto entry = packedTextureMap.find(packedTextureName);
            if (entry == packedTextureMap.end())
            {
                i32 subtextureIndex = atlases[packedImageDesc.atlasIndex]->addSubtexture(packedImageDesc.subimageIndex, params);
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
        void addTexture(ITexture* inTexture) 
        {
            singleton->m_textureMap.insert({ inTexture->name, inTexture });
            singleton->m_textures.push_back(inTexture);
        }

        void* m_objLoader;
        void* m_gltfLoader;
        tinygltf::TinyGLTF m_gltfImporter;

        // asset arrays for efficient iterating
        std::vector<Mesh*> m_meshes;
        std::vector<ITexture*> m_textures;

        // asset tables for efficient lookup
        std::unordered_map<std::string, std::unique_ptr<Scene>> m_sceneMap;
        std::unordered_map<std::string, ITexture*> m_textureMap;
        std::unordered_map<std::string, Mesh*> m_meshMap;
        std::unordered_map<std::string, Material> m_materialMap;
        std::unordered_map<std::string, MaterialTextureAtlas> m_packedMaterialMap;
    };
}