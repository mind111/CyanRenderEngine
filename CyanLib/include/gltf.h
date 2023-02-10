#pragma once

#include "glm/glm.hpp"
#include "tiny_gltf/json.hpp"

#include "Common.h"
#include "Asset.h"

namespace Cyan
{
    struct Scene;
    struct Entity;
    struct Triangles;

    namespace gltf
    {
        struct Scene
        {
            std::string name;
            std::vector<u32> nodes;
        };

        struct Node
        {
            // optional
            std::string name;
            i32 mesh = -1;
            i32 camera = -1;
            i32 hasMatrix = -1;
            i32 hasScale = -1;
            i32 hasRotation = -1;
            i32 hasTranslation = -1;
            std::array<f32, 16> matrix;
            glm::vec3 scale = glm::vec3(1.f);
            glm::vec4 rotation = glm::vec4(0.f, 0.f, 0.f, 1.f);
            glm::vec3 translation = glm::vec3(0.f);
            std::vector<u32> children;
        };

        struct Accessor
        {
            // required
            u32 componentType;
            u32 count;
            std::string type;
            // optional
            i32 bufferView = -1;
            u32 byteOffset = 0u;
            bool normalized = false;
            std::string name;
        };

        struct BufferView
        {
            // required
            u32 buffer;
            u32 byteLength;
            // optional
            u32 byteOffset = 0;
            u32 byteStride = 0;
            i32 target = -1;
            std::string name;
        };

        struct Buffer
        {
            // required
            u32 byteLength;
            // optional
            std::string uri;
            std::string name;
        };

        struct Attribute
        {
            // required
            u32 position;
            u32 normal;
            // optional
            i32 tangent = -1;
            i32 texCoord0 = -1;
            i32 texCoord1 = -1;
        };

        struct Primitive
        {
            // per gltf-2.0 spec
            enum class Mode
            {
                kPoints = 0,
                kLines,
                kLineLoop,
                kLineStrip,
                kTriangles,
                kTriangleStrip,
                kTriangleFan,
                kCount
            };
            
            Attribute attribute;
            i32 indices = -1;
            i32 material = -1;
            i32 mode = (i32)Mode::kTriangles; 
        };

        struct Mesh
        {
            // required
            std::vector<Primitive> primitives;
            // optional
            std::string name;
        };

        struct Image
        {
            // parsed form json
            std::string uri;
            i32 bufferView = -1;
            std::string mimeType;
            std::string name;
            // filled in when raw image data is interpreted using stbi
            i32 width, height, numChannels;
            bool bHdr = false;
            bool b16Bits = false;
            u8* pixels = nullptr;

            ~Image();
        };

        struct Sampler
        {
            enum class Filtering
            {
                NEAREST = 9728,
                LINEAR = 9729,
                NEAREST_MIPMAP_NEAREST = 9984,
                LINEAR_MIPMAP_NEAREST = 9985,
                NEAREST_MIPMAP_LINEAR = 9986,
                LINEAR_MIPMAP_LINEAR = 9987
            };
             
            enum class Wrap
            {
                CLAMP_TO_EDGE = 33071,
                REPEAT = 10497,
                MIRRORED_REPEAT = 33648,
            };

            u32 magFilter = (u32)Filtering::LINEAR;
            u32 minFilter = (u32)Filtering::LINEAR;
            u32 wrapS = 10497;
            u32 wrapT = 10497;
            std::string name;
        };

        struct Texture
        {
            i32 source = -1;
            i32 sampler = -1;
            std::string name;
        };

        // todo: handle extra fields such as "scale", "strength" etc
        struct TextureInfo
        {
            i32 index = -1;
            u32 texCoord = 0;
        };

        struct PbrMetallicRoughness
        {
            glm::vec4 baseColorFactor = glm::vec4(1.f, 1.f, 1.f, 1.f);
            TextureInfo baseColorTexture;
            f32 metallicFactor = 1.f;
            f32 roughnessFactor = 1.f;
            TextureInfo metallicRoughnessTexture;
        };

        struct Material
        {
            std::string name;
            PbrMetallicRoughness pbrMetallicRoughness;
            TextureInfo normalTexture;
            TextureInfo occlusionTexture;
            TextureInfo emissiveTexture;
        };

        // json parsing helpers
        using json = nlohmann::json;
        using json_iterator = nlohmann::json::iterator;
        using json_const_iterator = nlohmann::json::const_iterator;

        struct Glb : Asset::ExternalSource
        {
            struct ChunkDesc
            {
                u32 chunkLength;
                u32 chunkType;
            };

            Glb(const char* srcFilename);
            ~Glb() { }

            virtual void load() override;
            virtual void unload() override;

            void importScene(Cyan::Scene* outScene);
            void importAssets();

            bool bInitailized = false;
            ChunkDesc jsonChunkDesc;
            ChunkDesc binaryChunkDesc;
            u32 binaryChunkOffset;
            std::vector<u8> binaryChunk;
            // json object parsed from raw json string
            json o;
            u32 defaultScene = -1;
            std::vector<Scene> scenes;
            std::vector<Node> nodes;
            std::vector<Mesh> meshes;
            std::vector<Accessor> accessors;
            std::vector<Buffer> buffers;
            std::vector<BufferView> bufferViews;
            std::vector<Image> images;
            std::vector<Texture> textures;
            std::vector<Sampler> samplers;
            std::vector<Material> materials;
        private:
            void importMeshes();
            void importTextures();
            void importTexturesToAtlas();
            void importTexturesAsync();
            void importMaterials();
            void importPackedMaterials();
            void importNode(Cyan::Scene* outScene, Cyan::Entity* parent, const Node& node);
            void loadJsonChunk();

            void importTriangles(const gltf::Primitive& p, Triangles& outTriangles);
        };
    }
}