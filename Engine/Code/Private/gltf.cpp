#include <string>
#include <fstream>
#include <unordered_map>
#include <thread>

#include "gltf.h"
#include "stb_image.h"
#include "Geometry.h"

namespace glm 
{
    void from_json(const nlohmann::json& j, glm::vec3& v) 
    { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }

    void from_json(const nlohmann::json& j, glm::vec4& v)  
    { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }
}

namespace Cyan
{
    namespace gltf
    {
        bool jsonFind(json_const_iterator& outItr, const json& inJson, const char* m_name);
        bool jsonFind(json& outJsonObject, const json& inJson, const char* m_name);
        bool jsonGetString(std::string& outStr, const json& m_jsonObject);
        bool jsonGetBool(bool& outBool, const json& m_jsonObject);
        bool jsonGetInt(i32& outInt, const json& m_jsonObject);
        bool jsonGetUint(u32& outUint, const json& m_jsonObject);
        bool jsonGetVec3(glm::vec3& outVec3, const json& m_jsonObject);
        bool jsonGetVec4(glm::vec4& outVec4, const json& m_jsonObject);
        bool jsonGetFloat(f32& outFloat, const json& m_jsonObject);
        bool jsonFindAndGetVec3(glm::vec3& outVec3, const json& m_jsonObject, const char* m_name);
        bool jsonFindAndGetBool(bool& outBool, const json& m_jsonObject, const char* m_name);
        bool jsonFindAndGetVec4(glm::vec4& outVec3, const json& m_jsonObject, const char* m_name);
        bool jsonFindAndGetInt(i32& outInt, const json& m_jsonObject, const char* m_name);
        bool jsonFindAndGetUint(u32& outUint, const json& m_jsonObject, const char* m_name);
        bool jsonFindAndGetString(std::string& outStr, const json& m_jsonObject, const char* m_name);
        bool jsonFindAndGetFloat(f32& outFloat, const json& m_jsonObject, const char* m_name);

        static void from_json(const json& m_jsonObject, Scene& scene);
        static void from_json(const json& m_jsonObject, Node& node);
        static void from_json(const json& m_jsonObject, Buffer& buffer);
        static void from_json(const json& m_jsonObject, BufferView& bufferView);
        static void from_json(const json& m_jsonObject, Accessor& accessor);
        static void from_json(const json& m_jsonObject, Attribute& attribute);
        static void from_json(const json& m_jsonObject, Primitive& primitive);
        static void from_json(const json& m_jsonObject, Mesh& mesh);
        static void from_json(const json& m_jsonObject, Image& image);
        static void from_json(const json& m_jsonObject, Sampler& sampler);
        static void from_json(const json& m_jsonObject, Texture& texture);
        static void from_json(const json& m_jsonObject, TextureInfo& textureInfo);
        static void from_json(const json& m_jsonObject, PbrMetallicRoughness& pbrMetallicRoughness);
        static void from_json(const json& m_jsonObject, Material& material);

        Image::~Image()
        {
            if (pixels)
            {
                stbi_image_free(pixels);
            }
        }

#if 0
        void Gltf::importMaterials()
        {
            for (i32 i = 0; i < m_materials.size(); ++i)
            {
                const gltf::Material& gltfMatl = m_materials[i];
                auto matl = AssetManager::createMaterial(
                    gltfMatl.m_name.c_str(),
                    MATERIAL_SOURCE_PATH "M_DefaultOpaque_p.glsl",
                    [this, gltfMatl](MaterialInstance* defaultInstance) {
                        defaultInstance->setVec3("mp_albedo", gltfMatl.pbrMetallicRoughness.baseColorFactor);
                        defaultInstance->setFloat("mp_roughness", gltfMatl.pbrMetallicRoughness.roughnessFactor);
                        defaultInstance->setFloat("mp_metallic", gltfMatl.pbrMetallicRoughness.metallicFactor);
                        i32 baseColorTextureIndex = gltfMatl.pbrMetallicRoughness.baseColorTexture.index;
                        if (baseColorTextureIndex >= 0)
                        {
                            const gltf::Texture gltfTexture = m_textures[baseColorTextureIndex];
                            Texture2D* texture = AssetManager::findAsset<Texture2D>(gltfTexture.m_name.c_str()).get();
                            if (texture != nullptr)
                            {
                                defaultInstance->setTexture("mp_albedoMap", texture);
                                defaultInstance->setFloat("mp_hasAlbedoMap", 0.5f);
                            }
                        }

                        i32 normalTextureIndex = gltfMatl.normalTexture.index;
                        if (normalTextureIndex >= 0)
                        {
                            const gltf::Texture gltfTexture = m_textures[normalTextureIndex];
                            Texture2D* texture = AssetManager::findAsset<Texture2D>(gltfTexture.m_name.c_str()).get();
                            if (texture != nullptr)
                            {
                                defaultInstance->setTexture("mp_normalMap", texture);
                                defaultInstance->setFloat("mp_hasNormalMap", .5f);
                            }
                        }

                        i32 metallicRoughnessIndex = gltfMatl.pbrMetallicRoughness.metallicRoughnessTexture.index;
                        if (metallicRoughnessIndex >= 0)
                        {
                            const gltf::Texture gltfTexture = m_textures[metallicRoughnessIndex];
                            Texture2D* texture = AssetManager::findAsset<Texture2D>(gltfTexture.m_name.c_str()).get();
                            if (texture != nullptr)
                            {
                                defaultInstance->setTexture("mp_metallicRoughnessMap", texture);
                                defaultInstance->setFloat("mp_hasMetallicRoughnessMap", .5f);
                            }
                        }
                    }
                );
            }
        }
#endif

        Glb::Glb(const char* filename)
            : Gltf(filename)
        {
        }

        void Glb::load()
        {
            std::ifstream glb(m_filename, std::ios_base::binary);

            if (glb.is_open())
            {
                // read-in header data
                struct Header
                {
                    u32 magic;
                    u32 version;
                    u32 length;
                };
                Header header;
                glb.read(reinterpret_cast<char*>(&header), sizeof(Header));

                // per gltf-2.0 spec https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification-structure
                assert(header.magic == 0x46546C67);
                // read-in & parse the json chunk (reading this part as a whole could be slow ...?)
                ChunkDesc m_jsonChunkDesc;
                glb.read(reinterpret_cast<char*>(&m_jsonChunkDesc), sizeof(ChunkDesc));

                assert(m_jsonChunkDesc.chunkType == 0x4E4F534A);
                std::string jsonStr(m_jsonChunkDesc.chunkLength, ' ');
                glb.read(&jsonStr[0], m_jsonChunkDesc.chunkLength);

                // parse the json
                m_jsonObject = json::parse(jsonStr);

                // load json chunk
                loadJsonChunk();

                // load binary chunk
                glb.read(reinterpret_cast<char*>(&m_binaryChunkDesc), sizeof(m_binaryChunkDesc));
                m_binaryChunk.resize(m_binaryChunkDesc.chunkLength);
                glb.read(reinterpret_cast<char*>(m_binaryChunk.data()), m_binaryChunkDesc.chunkLength);
            }
        }

        void Glb::importTriangles(const gltf::Primitive& p, Triangles& outTriangles)
        {
            u32 numVertices = m_accessors[p.attribute.m_position].count;

            std::vector<Triangles::Vertex>& vertices = outTriangles.vertices;
            vertices.resize(numVertices);

            // determine whether the vertex attribute is tightly packed or interleaved
            bool bInterleaved = false;
            const gltf::Accessor& a = m_accessors[p.attribute.m_position]; 
            if (a.bufferView >= 0)
            {
                const gltf::BufferView& bv = m_bufferViews[a.bufferView];
                if (bv.byteStride > 0)
                {
                    bInterleaved = true;
                }
            }
            if (bInterleaved)
            {
                // todo: implement this code path
            }
            else 
            {
                glm::vec3* positions = nullptr;
                glm::vec3* normals = nullptr;
                glm::vec4* tangents = nullptr;
                glm::vec2* texCoord0 = nullptr;
                // position
                {
                    const gltf::Accessor& a = m_accessors[p.attribute.m_position]; 
                    assert(a.type == "VEC3");
                    if (a.bufferView >= 0)
                    {
                        const gltf::BufferView& bv = m_bufferViews[a.bufferView];
                        u32 offset = a.byteOffset + bv.byteOffset;
                        positions = reinterpret_cast<glm::vec3*>(m_binaryChunk.data() + offset);
                    }
                }
                // normal
                {
                    const gltf::Accessor& a = m_accessors[p.attribute.normal]; 
                    assert(a.type == "VEC3");
                    if (a.bufferView >= 0)
                    {
                        const gltf::BufferView& bv = m_bufferViews[a.bufferView];
                        u32 offset = a.byteOffset + bv.byteOffset;
                        normals = reinterpret_cast<glm::vec3*>(m_binaryChunk.data() + offset);
                    }
                }
                // tangents
                {
                    if (p.attribute.tangent >= 0)
                    {
                        const gltf::Accessor& a = m_accessors[p.attribute.tangent];
                        assert(a.type == "VEC4");
                        if (a.bufferView >= 0)
                        {
                            const gltf::BufferView& bv = m_bufferViews[a.bufferView];
                            u32 offset = a.byteOffset + bv.byteOffset;
                            tangents = reinterpret_cast<glm::vec4*>(m_binaryChunk.data() + offset);
                        }
                    }
                }
                // texCoord0
                {
                    if (p.attribute.texCoord0 >= 0)
                    {
                        const gltf::Accessor& a = m_accessors[p.attribute.texCoord0];
                        assert(a.type == "VEC2");
                        if (a.bufferView >= 0)
                        {
                            const gltf::BufferView& bv = m_bufferViews[a.bufferView];
                            u32 offset = a.byteOffset + bv.byteOffset;
                            texCoord0 = reinterpret_cast<glm::vec2*>(m_binaryChunk.data() + offset);
                        }
                    }
                }

                // fill vertices
                if (positions) 
                {
                    for (u32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].pos = positions[v];
                    }
                }
                if (normals) 
                {
                    for (u32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].normal = normals[v];
                    }
                }
                if (tangents) 
                {
                    for (u32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].tangent = tangents[v];
                    }
                }
                if (texCoord0) 
                {
                    for (u32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].texCoord0 = glm::vec2(texCoord0[v].x, 1.f - texCoord0[v].y);
                    }
                }

                std::vector<u32>& indices = outTriangles.indices;
                // fill indices
                if (p.indices >= 0)
                {
                    const gltf::Accessor& a = m_accessors[p.indices];
                    u32 numIndices = a.count;
                    indices.resize(numIndices);
                    if (a.bufferView >= 0)
                    {
                        const gltf::BufferView& bv = m_bufferViews[a.bufferView];
                        // would like to convert any other index data type to u32
                        if (a.componentType == 5121)
                        {
                            u8* dataAddress = reinterpret_cast<u8*>(m_binaryChunk.data() + a.byteOffset + bv.byteOffset);
                            for (u32 i = 0; i < numIndices; ++i)
                            {
                                indices[i] = static_cast<u32>(dataAddress[i]);
                            }
                        }
                        else if (a.componentType == 5123)
                        {
                            u16* dataAddress = reinterpret_cast<u16*>(m_binaryChunk.data() + a.byteOffset + bv.byteOffset);
                            for (u32 i = 0; i < numIndices; ++i)
                            {
                                indices[i] = static_cast<u32>(dataAddress[i]);
                            }
                        }
                        else if (a.componentType == 5125)
                        {
                            u32* dataAddress = reinterpret_cast<u32*>(m_binaryChunk.data() + a.byteOffset + bv.byteOffset);
                            memcpy(indices.data(), dataAddress, bv.byteLength);
                        }
                    }
                }
            }
        }

#if 0
        void Glb::importImage(const gltf::Image& gltfImage, Cyan::Image& outImage)
        {
            i32 bufferView = gltfImage.bufferView;
            // loading image data from a buffer
            if (bufferView >= 0)
            {
                const gltf::BufferView bv = m_bufferViews[bufferView];
                u8* dataAddress = m_binaryChunk.data() + bv.byteOffset;
                i32 hdr = stbi_is_hdr_from_memory(dataAddress, bv.byteLength);
                if (hdr)
                {
                    outImage.m_bitsPerChannel = 32;
                    outImage.m_pixels = std::shared_ptr<u8>(((u8*)stbi_loadf_from_memory(dataAddress, bv.byteLength, &outImage.m_width, &outImage.m_height, &outImage.m_numChannels, 0)));
                }
                else
                {
                    i32 is16Bit = stbi_is_16_bit_from_memory(dataAddress, bv.byteLength);
                    if (is16Bit)
                    {
                        outImage.m_bitsPerChannel = 16;
                        outImage.m_pixels = std::shared_ptr<u8>((u8*)stbi_load_16_from_memory(dataAddress, bv.byteLength, &outImage.m_width, &outImage.m_height, &outImage.m_numChannels, 0));
                    }
                    else
                    {
                        outImage.m_bitsPerChannel = 8;
                        outImage.m_pixels = std::shared_ptr<u8>((u8*)stbi_load_from_memory(dataAddress, bv.byteLength, &outImage.m_width, &outImage.m_height, &outImage.m_numChannels, 0));
                    }
                }
            }
            // todo: loading image data from an uri
            else
            {
            }
            assert(outImage.m_pixels);
            outImage.onLoaded();
        }

        void translateSampler(const gltf::Sampler& sampler, Sampler2D& outSampler, bool& bOutGenerateMipmap)
        {
            switch (sampler.magFilter)
            {
            case (u32)gltf::Sampler::Filtering::NEAREST: outSampler.magFilter = FM_POINT; break;
            case (u32)gltf::Sampler::Filtering::LINEAR: outSampler.magFilter = FM_BILINEAR; break;
            default: assert(0); break;
            }

            switch (sampler.minFilter)
            {
            case (u32)gltf::Sampler::Filtering::NEAREST: outSampler.minFilter = FM_POINT; break;
            case (u32)gltf::Sampler::Filtering::LINEAR: outSampler.minFilter = FM_BILINEAR; break;
            case (u32)gltf::Sampler::Filtering::LINEAR_MIPMAP_LINEAR: 
                outSampler.minFilter = FM_TRILINEAR; 
                bOutGenerateMipmap = true;
                break;
            case (u32)gltf::Sampler::Filtering::LINEAR_MIPMAP_NEAREST:
            case (u32)gltf::Sampler::Filtering::NEAREST_MIPMAP_LINEAR: 
            case (u32)gltf::Sampler::Filtering::NEAREST_MIPMAP_NEAREST: 
                assert(0);
                break;
            default: assert(0); break;
            }

            switch (sampler.wrapS)
            {
            case (u32)gltf::Sampler::Wrap::CLAMP_TO_EDGE: 
                outSampler.wrapS = WM_CLAMP; 
                break;
            case (u32)gltf::Sampler::Wrap::REPEAT: 
                outSampler.wrapS = WM_WRAP; 
                break;
            case (u32)gltf::Sampler::Wrap::MIRRORED_REPEAT:
            default: assert(0); break;
            }

            switch (sampler.wrapT)
            {
            case (u32)gltf::Sampler::Wrap::CLAMP_TO_EDGE: 
                outSampler.wrapT = WM_CLAMP; 
                break;
            case (u32)gltf::Sampler::Wrap::REPEAT: 
                outSampler.wrapT = WM_WRAP; 
                break;
            case (u32)gltf::Sampler::Wrap::MIRRORED_REPEAT:
            default: assert(0); break;
            }
        }
#endif

        void Glb::loadJsonChunk()
        {
            // 1. parse "scenes"
            json jScenes;
            if (jsonFind(jScenes, m_jsonObject, "scenes"))
            {
                if (jScenes.is_array())
                {
                    u32 numScenes = static_cast<u32>(jScenes.size());
                    m_scenes.resize(numScenes);
                    for (u32 i = 0; i < numScenes; ++i)
                    {
                        jScenes[i].get_to(m_scenes[i]);
                    }
                }
            }

            // 2. parse root scene 
            /** node - @min:
            * per gltf-2.0 documentation https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#gltf-basics
            * "An additional root-level property, scene (note singular), identifies which of the scenes
            * in the array SHOULD be displayed at load time. When scene is undefined, client
            * implementations MAY delay rendering until a particular scene is requested."
            * so basically, "scene" property defines the default scene to render at load time.
            */
            json jDefaultScene;
            if (jsonFind(jDefaultScene, m_jsonObject, "scene"))
            {
                if (jDefaultScene.is_number_unsigned())
                {
                    jDefaultScene.get_to(m_defaultScene);
                }
                else
                {
                    assert(0);
                }
            }

            // 3. load all nodes
            json jNodes;
            if (jsonFind(jNodes, m_jsonObject, "nodes"))
            {
                u32 numNodes = static_cast<u32>(jNodes.size());
                m_nodes.resize(numNodes);
                for (u32 i = 0; i < numNodes; ++i)
                {
                    jNodes[i].get_to(m_nodes[i]);
                }
            }

            // 4. load all bufferViews
            json jBufferViews;
            if (jsonFind(jBufferViews, m_jsonObject, "bufferViews"))
            {
                u32 numBufferViews = static_cast<u32>(jBufferViews.size());
                m_bufferViews.resize(numBufferViews);
                for (u32 i = 0; i < numBufferViews; ++i)
                {
                    jBufferViews[i].get_to(m_bufferViews[i]);
                }
            }

            // 5. load all accessors
            json jAccessors;
            if (jsonFind(jAccessors, m_jsonObject, "accessors"))
            {
                u32 numAccessors = static_cast<u32>(jAccessors.size());
                m_accessors.resize(numAccessors);
                for (u32 i = 0; i < numAccessors; ++i)
                {
                    jAccessors[i].get_to(m_accessors[i]);
                }
            }

            // 6: load all buffers (for .glb there should be only one buffer)
            json jBuffers;
            if (jsonFind(jBuffers, m_jsonObject, "buffers"))
            {
                u32 numBuffers = static_cast<u32>(jBuffers.size());
                m_buffers.resize(numBuffers);
                for (u32 i = 0; i < numBuffers; ++i)
                {
                    jBuffers[i].get_to(m_buffers[i]);
                }
            }

            // 7: load all meshes
            json jMeshes;
            if (jsonFind(jMeshes, m_jsonObject, "meshes"))
            {
                u32 numMeshes = static_cast<u32>(jMeshes.size());
                m_meshes.resize(numMeshes);
                for (u32 m = 0; m < numMeshes; ++m)
                {
                    jMeshes[m].get_to(m_meshes[m]);
                    // todo: this is just a hack for dealing with unnamed images
                    if (m_meshes[m].m_name.empty())
                    {
                        std::string prefix(m_filename);
                        m_meshes[m].m_name = prefix + "/mesh_" + std::to_string(m);
                    }
                }
            }

            // 8: load all images
            json jImages;
            if (jsonFind(jImages, m_jsonObject, "images"))
            {
                u32 numImages = static_cast<u32>(jImages.size());
                m_images.resize(numImages);
                for (u32 i = 0; i < numImages; ++i)
                {
                    jImages[i].get_to(m_images[i]);
                    // todo: this is just a hack for dealing with unnamed images
                    if (m_images[i].m_name.empty())
                    {
                        std::string prefix(m_filename);
                        m_images[i].m_name = prefix + "/image_" + std::to_string(i);
                    }
                }
            }

            // 9: load all samplers
            json jSamplers;
            if (jsonFind(jSamplers, m_jsonObject, "samplers"))
            {
                u32 numSamplers = static_cast<u32>(jSamplers.size());
                m_samplers.resize(numSamplers);
                for (u32 i = 0; i < numSamplers; ++i)
                {
                    jSamplers[i].get_to(m_samplers[i]);
                }
            }

            // 10: load all textures
            json jTextures;
            if (jsonFind(jTextures, m_jsonObject, "textures"))
            {
                u32 numTextures = static_cast<u32>(jTextures.size());
                m_textures.resize(numTextures);
                for (u32 i = 0; i < numTextures; ++i)
                {
                    jTextures[i].get_to(m_textures[i]);
                    // todo: this is just a hack for dealing with unnamed textures
                    if (m_textures[i].m_name.empty())
                    {
                        std::string prefix(m_filename);
                        m_textures[i].m_name = prefix + "/texture_" + std::to_string(i);
                    }
                }
            }

            // 11. load all materials
            json jMaterials;
            if (jsonFind(jMaterials, m_jsonObject, "materials"))
            {
                u32 numMaterials = static_cast<u32>(jMaterials.size());
                m_materials.resize(numMaterials);
                for (u32 i = 0; i < numMaterials; ++i)
                {
                    jMaterials[i].get_to(m_materials[i]);
                    // todo: this is just a hack for dealing with unnamed materials
                    if (m_materials[i].m_name.empty())
                    {
                        std::string prefix(m_filename);
                        m_materials[i].m_name = prefix + "/matl_" + std::to_string(i);
                    }
                }
            }
        }

        bool jsonFind(json_const_iterator& outItr, const json& inJson, const char* m_name)
        {
            outItr = inJson.find(m_name);
            return (outItr != inJson.end());
        }

        bool jsonFind(json& outJsonObject, const json& inJson, const char* m_name)
        {
            auto itr = inJson.find(m_name);
            if (itr != inJson.end())
            {
                outJsonObject = itr.value();
                return true;
            }
            return false;
        }

        bool jsonGetString(std::string& outStr, const json& m_jsonObject)
        {
            if (m_jsonObject.type() == json::value_t::string)
            {
                m_jsonObject.get_to(outStr);
                return true;
            }
            return false;
        }

        bool jsonGetBool(bool& outBool, const json& m_jsonObject)
        {
            if (m_jsonObject.type() == json::value_t::boolean)
            {
                m_jsonObject.get_to(outBool);
                return true;
            }
            return false;
        }

        bool jsonGetInt(i32& outInt, const json& m_jsonObject)
        {
            if (m_jsonObject.type() == json::value_t::number_integer)
            {
                m_jsonObject.get_to(outInt);
                return true;
            }
            return false;
        }

        bool jsonGetUint(u32& outUint, const json& m_jsonObject)
        {
            if (m_jsonObject.type() == json::value_t::number_unsigned)
            {
                m_jsonObject.get_to(outUint);
                return true;
            }
            return false;
        }

        bool jsonGetVec3(glm::vec3& outVec3, const json& m_jsonObject)
        {
            if (m_jsonObject.type() == json::value_t::array)
            {
                if (m_jsonObject.size() == 3)
                {
                    m_jsonObject.get_to(outVec3);
                    return true;
                }
            }
            return false;
        }

        bool jsonGetVec4(glm::vec4& outVec4, const json& m_jsonObject)
        {
            if (m_jsonObject.type() == json::value_t::array)
            {
                if (m_jsonObject.size() == 4)
                {
                    m_jsonObject.get_to(outVec4);
                    return true;
                }
            }
            return false;
        }

        bool jsonGetFloat(f32& outFloat, const json& m_jsonObject)
        {
            if (m_jsonObject.type() == json::value_t::number_float 
                || m_jsonObject.type() == json::value_t::number_unsigned 
                || m_jsonObject.type() == json::value_t::number_integer)
            {
                m_jsonObject.get_to(outFloat);
                return true;
            }
            return false;
        }

        bool jsonFindAndGetVec3(glm::vec3& outVec3, const json& m_jsonObject, const char* m_name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, m_jsonObject, m_name))
            {
                return jsonGetVec3(outVec3, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetBool(bool& outBool, const json& m_jsonObject, const char* m_name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, m_jsonObject, m_name))
            {
                return jsonGetBool(outBool, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetVec4(glm::vec4& outVec3, const json& m_jsonObject, const char* m_name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, m_jsonObject, m_name))
            {
                return jsonGetVec4(outVec3, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetInt(i32& outInt, const json& m_jsonObject, const char* m_name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, m_jsonObject, m_name))
            {
                return jsonGetInt(outInt, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetUint(u32& outUint, const json& m_jsonObject, const char* m_name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, m_jsonObject, m_name))
            {
                return jsonGetUint(outUint, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetString(std::string& outStr, const json& m_jsonObject, const char* m_name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, m_jsonObject, m_name))
            {
                return jsonGetString(outStr, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetFloat(f32& outFloat, const json& m_jsonObject, const char* m_name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, m_jsonObject, m_name))
            {
                return jsonGetFloat(outFloat, itr.value());
            }
            return false;
        }

        static void from_json(const json& m_jsonObject, Scene& scene)
        {
            jsonFindAndGetString(scene.m_name, m_jsonObject, "name");
            json jNodes;
            if (jsonFind(jNodes, m_jsonObject, "nodes"))
            {
                if (jNodes.is_array())
                {
                    jNodes.get_to(scene.m_nodes);
                }
            }
        }

        static void from_json(const json& m_jsonObject, Node& node)
        {
            jsonFindAndGetString(node.m_name, m_jsonObject, "name");
            u32 mesh;
            if (jsonFindAndGetUint(mesh, m_jsonObject, "mesh"))
            {
                node.mesh = (i32)mesh;
            }
            json matrix;
            if (jsonFind(matrix, m_jsonObject, "matrix"))
            {
                node.hasMatrix = 1;
                matrix.get_to(node.matrix);
            }
            json scale;
            if (jsonFind(scale, m_jsonObject, "scale"))
            {
                node.hasScale = 1;
                scale.get_to(node.scale);
            }
            json rotation;
            if (jsonFind(rotation, m_jsonObject, "rotation"))
            {
                node.hasRotation = 1;
                rotation.get_to(node.rotation);
            }
            json translation;
            if (jsonFind(translation, m_jsonObject, "translation"))
            {
                node.hasTranslation = 1;
                translation.get_to(node.translation);
            }
        }

        static void from_json(const json& m_jsonObject, Buffer& buffer)
        {
            // required
            jsonFindAndGetUint(buffer.byteLength, m_jsonObject, "byteLength");
            // optional
            jsonFindAndGetString(buffer.m_name, m_jsonObject, "name");
            jsonFindAndGetString(buffer.uri, m_jsonObject, "uri");
        }

        static void from_json(const json& m_jsonObject, BufferView& bufferView)
        {
            // required
            jsonFindAndGetUint(bufferView.buffer, m_jsonObject, "buffer");
            jsonFindAndGetUint(bufferView.byteLength, m_jsonObject, "byteLength");
            // optional
            jsonFindAndGetUint(bufferView.byteOffset, m_jsonObject, "byteOffset");
            jsonFindAndGetUint(bufferView.byteStride, m_jsonObject, "byteStride");
            u32 target;
            if (jsonFindAndGetUint(target, m_jsonObject, "target"))
            {
                bufferView.target = target;
            }
            jsonFindAndGetString(bufferView.m_name, m_jsonObject, "name");
        }

        static void from_json(const json& m_jsonObject, Accessor& accessor)
        {
            // required
            jsonFindAndGetUint(accessor.componentType, m_jsonObject, "componentType");
            jsonFindAndGetUint(accessor.count, m_jsonObject, "count");
            jsonFindAndGetString(accessor.type, m_jsonObject, "type");
            // optional
            u32 bufferView;
            if (jsonFindAndGetUint(bufferView, m_jsonObject, "bufferView"))
            {
                accessor.bufferView = bufferView;
            }
            jsonFindAndGetUint(accessor.byteOffset, m_jsonObject, "byteOffset");
            jsonFindAndGetString(accessor.m_name, m_jsonObject, "name");
            jsonFindAndGetBool(accessor.normalized, m_jsonObject, "normalized");
        }

        static void from_json(const json& m_jsonObject, Attribute& attribute)
        {
            if (!jsonFindAndGetUint(attribute.m_position, m_jsonObject, "POSITION"))
            {
                assert(0);
            }
            if (!jsonFindAndGetUint(attribute.normal, m_jsonObject, "NORMAL"))
            {
                assert(0);
            }
            u32 tangent;
            if (jsonFindAndGetUint(tangent, m_jsonObject, "TANGENT"))
            {
                attribute.tangent = (i32)tangent;
            }
            u32 texCoord0;
            if (jsonFindAndGetUint(texCoord0, m_jsonObject, "TEXCOORD_0"))
            {
                attribute.texCoord0 = (i32)texCoord0;
            }
            u32 texCoord1;
            if (jsonFindAndGetUint(texCoord1, m_jsonObject, "TEXCOORD_1"))
            {
                attribute.texCoord1 = (i32)texCoord1;
            }
        }

        static void from_json(const json& m_jsonObject, Primitive& primitive)
        {
            // required
            json_const_iterator itr;
            if (!jsonFind(itr, m_jsonObject, "attributes"))
            {
                assert(0);
            }
            itr.value().get_to(primitive.attribute);
            // optional
            u32 indices;
            if (jsonFindAndGetUint(indices, m_jsonObject, "indices"))
            {
                primitive.indices = (i32)indices;
            }
            u32 material;
            if (jsonFindAndGetUint(material, m_jsonObject, "material"))
            {
                primitive.material = (i32)material;
            }
            u32 mode;
            if (jsonFindAndGetUint(mode, m_jsonObject, "mode"))
            {
                primitive.mode = (i32)mode;
            }
        }

        static void from_json(const json& m_jsonObject, Mesh& mesh)
        {
            // required
            json jPrimitives;
            jsonFind(jPrimitives, m_jsonObject, "primitives");
            if (jPrimitives.is_array())
            {
                u32 numPrimitives = static_cast<u32>(jPrimitives.size());
                mesh.primitives.resize(numPrimitives);
                for (u32 i = 0; i < numPrimitives; ++i)
                {
                    jPrimitives[i].get_to(mesh.primitives[i]);
                }
            }
            // optional
            jsonFindAndGetString(mesh.m_name, m_jsonObject, "name");
        }

        static void from_json(const json& m_jsonObject, Image& image)
        {
            u32 bufferView;
            if (jsonFindAndGetUint(bufferView, m_jsonObject, "bufferView"))
            {
                image.bufferView = (i32)bufferView;
                if (!jsonFindAndGetString(image.mimeType, m_jsonObject, "mimeType"))
                {
                    // todo: issue error
                }
            }
            else
            {
                if (!jsonFindAndGetString(image.uri, m_jsonObject, "uri"))
                {
                    // todo: issue error
                }
            }
            jsonFindAndGetString(image.m_name, m_jsonObject, "name");
        }

        static void from_json(const json& m_jsonObject, Sampler& sampler)
        {
            jsonFindAndGetUint(sampler.magFilter, m_jsonObject, "magFilter");
            jsonFindAndGetUint(sampler.minFilter, m_jsonObject, "minFilter");
            jsonFindAndGetUint(sampler.wrapS, m_jsonObject, "wrapS");
            jsonFindAndGetUint(sampler.wrapT, m_jsonObject, "wrapT");
            jsonFindAndGetString(sampler.m_name, m_jsonObject, "name");
        }

        static void from_json(const json& m_jsonObject, Texture& texture)
        {
            u32 source;
            if (jsonFindAndGetUint(source, m_jsonObject, "source"))
            {
                texture.source = (i32)source;
            }
            u32 sampler;
            if (jsonFindAndGetUint(sampler, m_jsonObject, "sampler"))
            {
                texture.sampler = sampler;
            }
            jsonFindAndGetString(texture.m_name, m_jsonObject, "name");
        }

        static void from_json(const json& m_jsonObject, TextureInfo& textureInfo)
        {
            u32 index;
            if (jsonFindAndGetUint(index, m_jsonObject, "index"))
            {
                textureInfo.index = (i32)index;
            }
            jsonFindAndGetUint(textureInfo.texCoord, m_jsonObject, "texCoord");
        }

        static void from_json(const json& m_jsonObject, PbrMetallicRoughness& pbrMetallicRoughness)
        {
            json jBaseColorTexture;
            if (jsonFind(jBaseColorTexture, m_jsonObject, "baseColorTexture"))
            {
                jBaseColorTexture.get_to(pbrMetallicRoughness.baseColorTexture);
            }
            json jMetallicRoughness;
            if (jsonFind(jMetallicRoughness, m_jsonObject, "metallicRoughnessTexture"))
            {
                jMetallicRoughness.get_to(pbrMetallicRoughness.metallicRoughnessTexture);
            }
            jsonFindAndGetVec4(pbrMetallicRoughness.baseColorFactor, m_jsonObject, "baseColorFactor");
            jsonFindAndGetFloat(pbrMetallicRoughness.metallicFactor, m_jsonObject, "metallicFactor");
            jsonFindAndGetFloat(pbrMetallicRoughness.roughnessFactor, m_jsonObject, "roughnessFactor");
        }

        static void from_json(const json& m_jsonObject, Material& material)
        {
            jsonFindAndGetString(material.m_name, m_jsonObject, "name");
            json jPbrMetallicRoughness;
            if (jsonFind(jPbrMetallicRoughness, m_jsonObject, "pbrMetallicRoughness"))
            {
                jPbrMetallicRoughness.get_to(material.pbrMetallicRoughness);
            }
            json jNormalTexture;
            if (jsonFind(jNormalTexture, m_jsonObject, "normalTexture"))
            {
                jNormalTexture.get_to(material.normalTexture);
            }
            json jOcclusionTexture;
            if (jsonFind(jOcclusionTexture, m_jsonObject, "occlusionTexture"))
            {
                jOcclusionTexture.get_to(material.occlusionTexture);
            }
            json jEmissiveTexture;
            if (jsonFind(jEmissiveTexture, m_jsonObject, "emissiveTexture"))
            {
                jEmissiveTexture.get_to(material.emissiveTexture);
            }
            // todo: some fields are not loaded
        }
    }
}