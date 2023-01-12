#pragma once

#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#include <stbi/stb_image.h>
#include <tiny_gltf/json.hpp>
#include <tiny_gltf/tiny_gltf.h>

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
    class AssetManager {
    public:
        struct DefaultTextures 
        {
            Texture2DRenderable* checkerDark = nullptr;
            Texture2DRenderable* checkerOrange = nullptr;
            Texture2DRenderable* gridDark = nullptr;
            Texture2DRenderable* gridOrange = nullptr;
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
            ITextureRenderable::Spec spec = { };
            spec.numMips = 11u;
            ITextureRenderable::Parameter parameter = { };
            parameter.minificationFilter = FM_TRILINEAR;
            parameter.wrap_r = ITextureRenderable::Parameter::WrapMode::WRAP;
            parameter.wrap_s = ITextureRenderable::Parameter::WrapMode::WRAP;
            parameter.wrap_t = ITextureRenderable::Parameter::WrapMode::WRAP;
            m_defaultTextures.checkerDark = importTexture2D("default_checker_dark", ASSET_PATH "textures/defaults/checker_dark.png", spec, parameter);
            m_defaultTextures.checkerOrange = importTexture2D("default_checker_orange", ASSET_PATH "textures/defaults/checker_orange.png", spec, parameter);
            m_defaultTextures.gridDark = importTexture2D("default_grid_dark", ASSET_PATH "textures/defaults/grid_dark.png", spec, parameter);
            m_defaultTextures.gridOrange = importTexture2D("default_grid_orange", ASSET_PATH "textures/defaults/grid_orange.png", spec, parameter);
#undef DEFAULT_TEXTURE_FOLDER

            /**
                initialize the default material 
            */ 
            createMaterial("DefaultMaterial");
        }

        static AssetManager* get() { return singleton; }

        void importGltfNode(Scene* scene, tinygltf::Model& model, Entity* parent, tinygltf::Node& node);
        Mesh* importGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh); 
        Cyan::Texture2DRenderable* importGltfTexture(const char* nodeName, tinygltf::Model& model, i32 index);
        void importGltfTextures(const char* nodeName, tinygltf::Model& model);
        static void importGltf(Scene* scene, const char* filename, const char* name=nullptr);
        std::vector<ISubmesh*> importObj(const char* baseDir, const char* filename);
        void importScene(Scene* scene, const char* file);
        void importEntities(Scene* scene, const nlohmann::basic_json<std::map>& entityInfoList);
        void importTextures(const nlohmann::basic_json<std::map>& textureInfoList);
        Mesh* importMesh(Scene* scene, std::string& path, const char* name, bool bNormalize);
        void importMeshes(Scene* scene, const nlohmann::basic_json<std::map>& meshInfoList);

        /**
        * Creating a texture from scratch; `name` must be unique
        */
        static Texture2DRenderable* createTexture2D(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ }) {
            Texture2DRenderable* outTexture = getAsset<Texture2DRenderable>(name);
            if (!outTexture)
            {
                outTexture = new Texture2DRenderable(name, spec, parameter);
                singleton->addTexture(outTexture);
            }
            return outTexture;
        }

        static Texture3DRenderable* createTexture3D(const char* name, const ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ }) {
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
                outTexture = new TextureCubeRenderable(name, spec, parameter);
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
        static Texture2DRenderable* importTexture2D(const char* name, const char* filename, ITextureRenderable::Spec& spec, ITextureRenderable::Parameter parameter=ITextureRenderable::Parameter{ })
        {
            // todo: this is not a robust solutions to this!!! a better way maybe parse the file header to get the true file format
            // determine whether the given image is ldr or hdr based on file extension

            std::string path(filename);
            u32 found = path.find_last_of('.');
            std::string extension = path.substr(found, found + 1);

            int width, height, numChannels;
            stbi_set_flip_vertically_on_load(1);

            if (extension == ".hdr")
            {
                spec.pixelData = reinterpret_cast<u8*>(stbi_loadf(filename, &width, &height, &numChannels, 0));
                // todo: pixel format is hard coded for now
                if (numChannels == 3)
                {
                    spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
                }
                else if (numChannels == 4)
                {
                    spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGBA16F;
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
                    spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB8;
                }
                else if (numChannels == 4)
                {
                    spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGBA8;
                }
                spec.width = width;
                spec.height = height;
            }
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
        static Material* getAsset<Material>(const char* name) {
            auto entry = singleton->m_materialMap.find(std::string(name));
            if (entry != singleton->m_materialMap.end()) {
                return &entry->second;
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
        void addTexture(ITextureRenderable* inTexture) {
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
        std::unordered_map<std::string, Material> m_materialMap;
    };
}