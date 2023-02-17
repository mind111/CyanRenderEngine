#include <string>
#include <fstream>
#include <unordered_map>
#include <thread>

#include "gltf.h"
#include "stbi/stb_image.h"
#include "Entity.h"
#include "ExternalAssetFile.h"
#include "AssetManager.h"
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
        bool jsonFind(json_const_iterator& outItr, const json& inJson, const char* name);
        bool jsonFind(json& outJsonObject, const json& inJson, const char* name);
        bool jsonGetString(std::string& outStr, const json& o);
        bool jsonGetBool(bool& outBool, const json& o);
        bool jsonGetInt(i32& outInt, const json& o);
        bool jsonGetUint(u32& outUint, const json& o);
        bool jsonGetVec3(glm::vec3& outVec3, const json& o);
        bool jsonGetVec4(glm::vec4& outVec4, const json& o);
        bool jsonGetFloat(f32& outFloat, const json& o);
        bool jsonFindAndGetVec3(glm::vec3& outVec3, const json& o, const char* name);
        bool jsonFindAndGetBool(bool& outBool, const json& o, const char* name);
        bool jsonFindAndGetVec4(glm::vec4& outVec3, const json& o, const char* name);
        bool jsonFindAndGetInt(i32& outInt, const json& o, const char* name);
        bool jsonFindAndGetUint(u32& outUint, const json& o, const char* name);
        bool jsonFindAndGetString(std::string& outStr, const json& o, const char* name);
        bool jsonFindAndGetFloat(f32& outFloat, const json& o, const char* name);

        static void from_json(const json& o, Scene& scene);
        static void from_json(const json& o, Node& node);
        static void from_json(const json& o, Buffer& buffer);
        static void from_json(const json& o, BufferView& bufferView);
        static void from_json(const json& o, Accessor& accessor);
        static void from_json(const json& o, Attribute& attribute);
        static void from_json(const json& o, Primitive& primitive);
        static void from_json(const json& o, Mesh& mesh);
        static void from_json(const json& o, Image& image);
        static void from_json(const json& o, Sampler& sampler);
        static void from_json(const json& o, Texture& texture);
        static void from_json(const json& o, TextureInfo& textureInfo);
        static void from_json(const json& o, PbrMetallicRoughness& pbrMetallicRoughness);
        static void from_json(const json& o, Material& material);

        Image::~Image()
        {
            if (pixels)
            {
                stbi_image_free(pixels);
            }
        }

        Glb::Glb(const char* inFilename)
            : Gltf(inFilename)
        {
        }

        void Glb::load()
        {
            std::ifstream glb(filename, std::ios_base::binary);

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
                ChunkDesc jsonChunkDesc;
                glb.read(reinterpret_cast<char*>(&jsonChunkDesc), sizeof(ChunkDesc));

                assert(jsonChunkDesc.chunkType == 0x4E4F534A);
                std::string jsonStr(" ", jsonChunkDesc.chunkLength);
                glb.read(&jsonStr[0], jsonChunkDesc.chunkLength);

                // parse the json
                o = json::parse(jsonStr);

                // load json chunk
                loadJsonChunk();

                // load binary chunk
                glb.read(reinterpret_cast<char*>(&binaryChunkDesc), sizeof(binaryChunkDesc));
                binaryChunk.resize(binaryChunkDesc.chunkLength);
                glb.read(reinterpret_cast<char*>(binaryChunk.data()), binaryChunkDesc.chunkLength);
            }
        }

        void Glb::unload()
        {
            // todo: clear all loaded data
        }

        void Glb::importScene(Cyan::Scene* outScene)
        {
            importAssets();

            // load scene hierarchy 
            if (defaultScene >= 0)
            {
                const gltf::Scene& gltfScene = scenes[defaultScene];
                for (i32 i = 0; i < gltfScene.nodes.size(); ++i)
                {
                    const gltf::Node& node = nodes[gltfScene.nodes[i]];
                    importNode(outScene, nullptr, node);
                }
            }
        }

        void Glb::importAssets()
        {
            importMeshes();
#if BINDLESS_TEXTURE   
            importTextures();
            importMaterials();
#else
            importTexturesToAtlas();
            importPackedMaterials();
#endif
        }

        void Glb::importTriangles(const gltf::Primitive& p, Triangles& outTriangles)
        {
            u32 numVertices = accessors[p.attribute.position].count;

            std::vector<Triangles::Vertex>& vertices = outTriangles.vertices;
            vertices.resize(numVertices);

            // determine whether the vertex attribute is tightly packed or interleaved
            bool bInterleaved = false;
            const gltf::Accessor& a = accessors[p.attribute.position]; 
            if (a.bufferView >= 0)
            {
                const gltf::BufferView& bv = bufferViews[a.bufferView];
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
                    const gltf::Accessor& a = accessors[p.attribute.position]; 
                    assert(a.type == "VEC3");
                    if (a.bufferView >= 0)
                    {
                        const gltf::BufferView& bv = bufferViews[a.bufferView];
                        u32 offset = a.byteOffset + bv.byteOffset;
                        positions = reinterpret_cast<glm::vec3*>(binaryChunk.data() + offset);
                    }
                }
                // normal
                {
                    const gltf::Accessor& a = accessors[p.attribute.normal]; 
                    assert(a.type == "VEC3");
                    if (a.bufferView >= 0)
                    {
                        const gltf::BufferView& bv = bufferViews[a.bufferView];
                        u32 offset = a.byteOffset + bv.byteOffset;
                        normals = reinterpret_cast<glm::vec3*>(binaryChunk.data() + offset);
                    }
                }
                // tangents
                {
                    if (p.attribute.tangent >= 0)
                    {
                        const gltf::Accessor& a = accessors[p.attribute.tangent];
                        assert(a.type == "VEC4");
                        if (a.bufferView >= 0)
                        {
                            const gltf::BufferView& bv = bufferViews[a.bufferView];
                            u32 offset = a.byteOffset + bv.byteOffset;
                            tangents = reinterpret_cast<glm::vec4*>(binaryChunk.data() + offset);
                        }
                    }
                }
                // texCoord0
                {
                    if (p.attribute.texCoord0 >= 0)
                    {
                        const gltf::Accessor& a = accessors[p.attribute.texCoord0];
                        assert(a.type == "VEC2");
                        if (a.bufferView >= 0)
                        {
                            const gltf::BufferView& bv = bufferViews[a.bufferView];
                            u32 offset = a.byteOffset + bv.byteOffset;
                            texCoord0 = reinterpret_cast<glm::vec2*>(binaryChunk.data() + offset);
                        }
                    }
                }

                // fill vertices
                if (positions) 
                {
                    for (i32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].pos = positions[v];
                    }
                }
                if (normals) 
                {
                    for (i32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].normal = normals[v];
                    }
                }
                if (tangents) 
                {
                    for (i32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].tangent = tangents[v];
                    }
                }
                if (texCoord0) 
                {
                    for (i32 v = 0; v < numVertices; ++v)
                    {
                        vertices[v].texCoord0 = glm::vec2(texCoord0[v].x, 1.f - texCoord0[v].y);
                    }
                }

                std::vector<u32>& indices = outTriangles.indices;
                // fill indices
                if (p.indices >= 0)
                {
                    const gltf::Accessor& a = accessors[p.indices];
                    u32 numIndices = a.count;
                    indices.resize(numIndices);
                    if (a.bufferView >= 0)
                    {
                        const gltf::BufferView& bv = bufferViews[a.bufferView];
                        // would like to convert any other index data type to u32
                        if (a.componentType == 5121)
                        {
                            u8* dataAddress = reinterpret_cast<u8*>(binaryChunk.data() + a.byteOffset + bv.byteOffset);
                            for (i32 i = 0; i < numIndices; ++i)
                            {
                                indices[i] = static_cast<u32>(dataAddress[i]);
                            }
                        }
                        else if (a.componentType == 5123)
                        {
                            u16* dataAddress = reinterpret_cast<u16*>(binaryChunk.data() + a.byteOffset + bv.byteOffset);
                            for (i32 i = 0; i < numIndices; ++i)
                            {
                                indices[i] = static_cast<u32>(dataAddress[i]);
                            }
                        }
                        else if (a.componentType == 5125)
                        {
                            u32* dataAddress = reinterpret_cast<u32*>(binaryChunk.data() + a.byteOffset + bv.byteOffset);
                            memcpy(indices.data(), dataAddress, bv.byteLength);
                        }
                    }
                }
            }
        }

        void Glb::importMeshes()
        {
            u32 numMeshes = meshes.size();
            for (i32 m = 0; m < numMeshes; ++m)
            {
                auto mesh = AssetManager::createStaticMesh(meshes[m].name.c_str());
                // make sure that we are not re-importing same mesh multiple times
                assert(mesh->numSubmeshes() == 0);
                i32 numSubmeshes = meshes[m].primitives.size();
                for (i32 sm = 0; sm < numSubmeshes; ++sm)
                {
                    const gltf::Primitive& p = meshes[m].primitives[sm];

                    Geometry* geometry = nullptr;
                    switch ((Primitive::Mode)p.mode)
                    {
                    case Primitive::Mode::kTriangles: {
                        // todo: this also needs to be tracked and managed by the AssetManager using some kind of GUID system
                        geometry = new Triangles();
                        Triangles* triangles = dynamic_cast<Triangles*>(geometry);
                        assert(triangles);
                        importTriangles(p, *triangles);
                        mesh->addSubmeshDeferred(triangles);
                    } break;
                    case Primitive::Mode::kLines:
                    case Primitive::Mode::kPoints:
                    default:
                        assert(0);
                    }
                }
            }
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

        // todo: handle the case for external images
        // todo: track the memory usage during importing
        void Glb::importTextures()
        { 
            ScopedTimer timer("loadTextures()", true);
            // 1. load all the images into memory first assuming that it all fit
            for (i32 i = 0; i < images.size(); ++i)
            {
                gltf::Image& gltfImage = images[i];
                i32 bufferView = gltfImage.bufferView;
                if (bufferView >= 0)
                {
                    const gltf::BufferView bv = bufferViews[bufferView];
                    u8* dataAddress = binaryChunk.data() + bv.byteOffset;
                    AssetManager::importImage(gltfImage.name.c_str(), dataAddress, bv.byteLength);
                }
                else
                {
                    // for .glb the images are embedded so a bufferView is required
                    assert(0);
                }
            }

            // 2. load all textures
            for (i32 i = 0; i < textures.size(); ++i)
            {
                const gltf::Texture& texture = textures[i];
                const gltf::Image& gltfImage = images[texture.source];
                const gltf::Sampler& gltfSampler = samplers[texture.sampler];

                Cyan::Image* image = AssetManager::getAsset<Cyan::Image>(gltfImage.name.c_str());

                Sampler2D sampler;
                bool bGenerateMipmap = false;
                translateSampler(gltfSampler, sampler, bGenerateMipmap);
                AssetManager::createTexture2DBindless(texture.name.c_str(), image, sampler);
            }
        }

        void Glb::importTexturesToAtlas()
        {
            auto assetManager = AssetManager::get();

            ScopedTimer timer("loadTextures()", true);

            // 1. load all the images into memory first assuming that it all fit
            for (i32 i = 0; i < images.size(); ++i)
            {
                gltf::Image& gltfImage = images[i];
                i32 bufferView = gltfImage.bufferView;
                if (bufferView >= 0)
                {
                    const gltf::BufferView bv = bufferViews[bufferView];
                    u8* dataAddress = binaryChunk.data() + bv.byteOffset;
                    Cyan::Image* outImage = AssetManager::createImage(gltfImage.name.c_str(), dataAddress, bv.byteLength); 
                    PackedImageDesc packedImageDesc = assetManager->packImage(outImage);
                }
                else
                {
                    // for .glb the images are embedded so a bufferView is required
                    assert(0);
                }
            }

            // 2. load all the textures
            for (i32 i = 0; i < textures.size(); ++i)
            {
                const gltf::Texture& texture = textures[i];
                gltf::Image& gltfImage = images[texture.source];
                const gltf::Sampler& gltfSampler = samplers[texture.sampler];
                PackedImageDesc packedImageDesc = assetManager->getPackedImageDesc(gltfImage.name.c_str());
                Sampler2D sampler;
                bool bGenerateMipmap;
                translateSampler(gltfSampler, sampler, bGenerateMipmap);
                assetManager->addPackedTexture(texture.name.c_str(), packedImageDesc, bGenerateMipmap, sampler);
            }
        }

        void Glb::importPackedMaterials() 
        {
#if 0
            ScopedTimer timer("importPackedMaterials()", true);
            auto assetManager = AssetManager::get();
            for (i32 i = 0; i < materials.size(); ++i)
            {
                const gltf::Material& gltfMatl = materials[i];
                // todo: this is just a hack, need to come up with a better way to deal with no name assets in gltf
                Cyan::MaterialTextureAtlas& matl = AssetManager::createPackedMaterial(gltfMatl.name.c_str());
                matl.albedo = gltfMatl.pbrMetallicRoughness.baseColorFactor;
                matl.roughness = gltfMatl.pbrMetallicRoughness.roughnessFactor;
                matl.metallic = gltfMatl.pbrMetallicRoughness.metallicFactor;
                i32 baseColorTextureIndex = gltfMatl.pbrMetallicRoughness.baseColorTexture.index;
                if (baseColorTextureIndex >= 0)
                {
                    const gltf::Texture texture = textures[baseColorTextureIndex];
                    auto outDesc = assetManager->getPackedTextureDesc(texture.name.c_str());
                    assert(outDesc.atlasIndex >= 0 && outDesc.subtextureIndex >= 0);
                    matl.albedoMap = outDesc;
                }

                i32 metallicRoughnessIndex = gltfMatl.pbrMetallicRoughness.metallicRoughnessTexture.index;
                if (metallicRoughnessIndex >= 0)
                {
                    const gltf::Texture texture = textures[metallicRoughnessIndex];
                    auto outDesc = assetManager->getPackedTextureDesc(texture.name.c_str());
                    assert(outDesc.atlasIndex >= 0 && outDesc.subtextureIndex >= 0);
                    matl.metallicRoughnessMap = outDesc;
                }

                i32 normalTextureIndex = gltfMatl.normalTexture.index;
                if (normalTextureIndex >= 0)
                {
                    const gltf::Texture texture = textures[normalTextureIndex];
                    auto outDesc = assetManager->getPackedTextureDesc(texture.name.c_str());
                    assert(outDesc.atlasIndex >= 0 && outDesc.subtextureIndex >= 0);
                    matl.normalMap = outDesc;
                }
            }
#endif
        }

        void Glb::importTexturesAsync()
        {
            auto loadImagesTask = [this](const Glb& inGlb) {
                Glb glb(inGlb);
                for (i32 i = 0; i < glb.images.size(); ++i)
                {
                    gltf::Image& image = glb.images[i];

                    // load one image
                    i32 bufferView = image.bufferView;
                    if (bufferView >= 0)
                    {
                        const gltf::BufferView bv = bufferViews[bufferView];
                        u8* dataAddress = binaryChunk.data() + bv.byteOffset;
                        i32 hdr = stbi_is_hdr_from_memory(dataAddress, bv.byteLength);
                        if (hdr)
                        {
                            image.bHdr = true;
                            image.pixels = reinterpret_cast<u8*>(stbi_loadf_from_memory(dataAddress, bv.byteLength, &image.width, &image.height, &image.numChannels, 0));
                        }
                        else
                        {
                            i32 is16Bit = stbi_is_16_bit_from_memory(dataAddress, bv.byteLength);
                            if (is16Bit)
                            {
                                image.b16Bits = true;
                            }
                            else
                            {
                                image.pixels = stbi_load_from_memory(dataAddress, bv.byteLength, &image.width, &image.height, &image.numChannels, 0);
                            }
                        }
                    }

                    // let the main thread know that an image is finished loading
                }
            };

            std::thread thread(loadImagesTask, *this);
        }

        void Glb::importMaterials()
        {
            ScopedTimer timer("importMaterials()", true);
            for (i32 i = 0; i < materials.size(); ++i)
            {
                const gltf::Material& gltfMatl = materials[i];
                // todo: this is just a hack, need to come up with a better way to deal with no name assets in gltf
                Cyan::MaterialBindless* matl = AssetManager::createMaterialBindless(gltfMatl.name.c_str());
                matl->albedo = gltfMatl.pbrMetallicRoughness.baseColorFactor;
                matl->roughness = gltfMatl.pbrMetallicRoughness.roughnessFactor;
                matl->metallic = gltfMatl.pbrMetallicRoughness.metallicFactor;
                i32 baseColorTextureIndex = gltfMatl.pbrMetallicRoughness.baseColorTexture.index;
                if (baseColorTextureIndex >= 0)
                {
                    const gltf::Texture texture = textures[baseColorTextureIndex];
                    matl->albedoMap = AssetManager::getAsset<Texture2DBindless>(texture.name.c_str());
                }

                i32 metallicRoughnessIndex = gltfMatl.pbrMetallicRoughness.metallicRoughnessTexture.index;
                if (metallicRoughnessIndex >= 0)
                {
                    const gltf::Texture texture = textures[metallicRoughnessIndex];
                    matl->metallicRoughnessMap = AssetManager::getAsset<Texture2DBindless>(texture.name.c_str());
                }

                i32 normalTextureIndex = gltfMatl.normalTexture.index;
                if (normalTextureIndex >= 0)
                {
                    const gltf::Texture texture = textures[normalTextureIndex];
                    matl->normalMap = AssetManager::getAsset<Texture2DBindless>(texture.name.c_str());
                }
            }
        }

        void Glb::importNode(Cyan::Scene* scene, Cyan::Entity* parent, const Node& node)
        {
            // @name
            std::string name;
            // @transform
            Transform t;
            if (node.hasMatrix >= 0)
            {
                const std::array<f32, 16>& m = node.matrix;
                glm::mat4 mat = {
                    glm::vec4(m[0],  m[1],  m[2],  m[3]),     // column 0
                    glm::vec4(m[4],  m[5],  m[6],  m[7]),     // column 1
                    glm::vec4(m[8],  m[9],  m[10], m[11]),    // column 2
                    glm::vec4(m[12], m[13], m[14], m[15])     // column 3
                };
                t.fromMatrix(mat);
            }
            else
            {
                glm::vec3 scale(1.f); glm::vec4 rotation(0.f, 0.f, 0.f, 1.f); glm::vec3 translation(0.f);
                // @scale
                if (node.hasScale >= 0)
                {
                    scale = node.scale;
                }
                // @rotation
                if (node.hasRotation >= 0)
                {
                    rotation = node.rotation;
                }
                // @translation
                if (node.hasTranslation >= 0)
                {
                    translation = node.translation;
                }
                t.m_scale = scale;
                t.m_qRot = glm::quat(rotation.w, glm::vec3(rotation.x, rotation.y, rotation.z));
                t.m_translate = translation;
            }
            // @mesh
            Entity* e = nullptr;
            if (node.mesh >= 0)
            {
               const gltf::Mesh& gltfMesh = meshes[node.mesh];
               Cyan::StaticMesh* mesh = AssetManager::getAsset<Cyan::StaticMesh>(gltfMesh.name.c_str());
               StaticMeshEntity* staticMeshEntity = scene->createStaticMeshEntity(name.c_str(), t, mesh, parent);
               staticMeshEntity->setMaterial(AssetManager::getAsset<Cyan::Material>("DefaultMaterial"));
               e = staticMeshEntity;
               for (i32 p = 0; p < gltfMesh.primitives.size(); ++p)
               {
                   const gltf::Primitive& primitive = gltfMesh.primitives[p];
                   if (primitive.material >= 0)
                   {
                       const gltf::Material& gltfMatl = materials[primitive.material];
                       auto matl = AssetManager::getAsset<Cyan::Material>(gltfMatl.name.c_str());
                       staticMeshEntity->setMaterial(matl, p);
                   }
               }
            }
            else
            {
               e = scene->createEntity(name.c_str(), t, parent);
            }

            // recurse into children nodes
            for (auto child : node.children)
            {
                importNode(scene, e, nodes[child]);
            }
        }

        void Glb::loadJsonChunk()
        {
            // 1. parse "scenes"
            json jScenes;
            if (jsonFind(jScenes, o, "scenes"))
            {
                if (jScenes.is_array())
                {
                    u32 numScenes = jScenes.size();
                    scenes.resize(numScenes);
                    for (i32 i = 0; i < numScenes; ++i)
                    {
                        jScenes[i].get_to(scenes[i]);
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
            if (jsonFind(jDefaultScene, o, "scene"))
            {
                if (jDefaultScene.is_number_unsigned())
                {
                    jDefaultScene.get_to(defaultScene);
                }
                else
                {
                    assert(0);
                }
            }

            // 3. load all nodes
            json jNodes;
            if (jsonFind(jNodes, o, "nodes"))
            {
                u32 numNodes = jNodes.size();
                nodes.resize(numNodes);
                for (i32 i = 0; i < numNodes; ++i)
                {
                    jNodes[i].get_to(nodes[i]);
                }
            }

            // 4. load all bufferViews
            json jBufferViews;
            if (jsonFind(jBufferViews, o, "bufferViews"))
            {
                u32 numBufferViews = jBufferViews.size();
                bufferViews.resize(numBufferViews);
                for (i32 i = 0; i < numBufferViews; ++i)
                {
                    jBufferViews[i].get_to(bufferViews[i]);
                }
            }

            // 5. load all accessors
            json jAccessors;
            if (jsonFind(jAccessors, o, "accessors"))
            {
                u32 numAccessors = jAccessors.size();
                accessors.resize(numAccessors);
                for (i32 i = 0; i < numAccessors; ++i)
                {
                    jAccessors[i].get_to(accessors[i]);
                }
            }

            // 6: load all buffers (for .glb there should be only one buffer)
            json jBuffers;
            if (jsonFind(jBuffers, o, "buffers"))
            {
                u32 numBuffers = jBuffers.size();
                buffers.resize(numBuffers);
                for (i32 i = 0; i < numBuffers; ++i)
                {
                    jBuffers[i].get_to(buffers[i]);
                }
            }

            // 7: load all meshes
            json jMeshes;
            if (jsonFind(jMeshes, o, "meshes"))
            {
                u32 numMeshes = jMeshes.size();
                meshes.resize(numMeshes);
                for (i32 m = 0; m < numMeshes; ++m)
                {
                    jMeshes[m].get_to(meshes[m]);
                    // todo: this is just a hack for dealing with unnamed images
                    if (meshes[m].name.empty())
                    {
                        std::string prefix(filename);
                        meshes[m].name = prefix + "/mesh_" + std::to_string(m);
                    }
                }
            }

            // 8: load all images
            json jImages;
            if (jsonFind(jImages, o, "images"))
            {
                u32 numImages = jImages.size();
                images.resize(numImages);
                for (i32 i = 0; i < numImages; ++i)
                {
                    jImages[i].get_to(images[i]);
                    // todo: this is just a hack for dealing with unnamed images
                    if (images[i].name.empty())
                    {
                        std::string prefix(filename);
                        images[i].name = prefix + "/image_" + std::to_string(i);
                    }
                }
            }

            // 9: load all samplers
            json jSamplers;
            if (jsonFind(jSamplers, o, "samplers"))
            {
                u32 numSamplers = jSamplers.size();
                samplers.resize(numSamplers);
                for (i32 i = 0; i < numSamplers; ++i)
                {
                    jSamplers[i].get_to(samplers[i]);
                }
            }

            // 10: load all textures
            json jTextures;
            if (jsonFind(jTextures, o, "textures"))
            {
                u32 numTextures = jTextures.size();
                textures.resize(numTextures);
                for (i32 i = 0; i < numTextures; ++i)
                {
                    jTextures[i].get_to(textures[i]);
                    // todo: this is just a hack for dealing with unnamed textures
                    if (textures[i].name.empty())
                    {
                        std::string prefix(filename);
                        textures[i].name = prefix + "/texture_" + std::to_string(i);
                    }
                }
            }

            // 11. load all materials
            json jMaterials;
            if (jsonFind(jMaterials, o, "materials"))
            {
                u32 numMaterials = jMaterials.size();
                materials.resize(numMaterials);
                for (i32 i = 0; i < numMaterials; ++i)
                {
                    jMaterials[i].get_to(materials[i]);
                    // todo: this is just a hack for dealing with unnamed materials
                    if (materials[i].name.empty())
                    {
                        std::string prefix(filename);
                        materials[i].name = prefix + "/matl_" + std::to_string(i);
                    }
                }
            }
        }

        bool jsonFind(json_const_iterator& outItr, const json& inJson, const char* name)
        {
            outItr = inJson.find(name);
            return (outItr != inJson.end());
        }

        bool jsonFind(json& outJsonObject, const json& inJson, const char* name)
        {
            auto itr = inJson.find(name);
            if (itr != inJson.end())
            {
                outJsonObject = itr.value();
                return true;
            }
            return false;
        }

        bool jsonGetString(std::string& outStr, const json& o)
        {
            if (o.type() == json::value_t::string)
            {
                o.get_to(outStr);
                return true;
            }
            return false;
        }

        bool jsonGetBool(bool& outBool, const json& o)
        {
            if (o.type() == json::value_t::boolean)
            {
                o.get_to(outBool);
                return true;
            }
            return false;
        }

        bool jsonGetInt(i32& outInt, const json& o)
        {
            if (o.type() == json::value_t::number_integer)
            {
                o.get_to(outInt);
                return true;
            }
            return false;
        }

        bool jsonGetUint(u32& outUint, const json& o)
        {
            if (o.type() == json::value_t::number_unsigned)
            {
                o.get_to(outUint);
                return true;
            }
            return false;
        }

        bool jsonGetVec3(glm::vec3& outVec3, const json& o)
        {
            if (o.type() == json::value_t::array)
            {
                if (o.size() == 3)
                {
                    o.get_to(outVec3);
                    return true;
                }
            }
            return false;
        }

        bool jsonGetVec4(glm::vec4& outVec4, const json& o)
        {
            if (o.type() == json::value_t::array)
            {
                if (o.size() == 4)
                {
                    o.get_to(outVec4);
                    return true;
                }
            }
            return false;
        }

        bool jsonGetFloat(f32& outFloat, const json& o)
        {
            if (o.type() == json::value_t::number_float 
                || o.type() == json::value_t::number_unsigned 
                || o.type() == json::value_t::number_integer)
            {
                o.get_to(outFloat);
                return true;
            }
            return false;
        }

        bool jsonFindAndGetVec3(glm::vec3& outVec3, const json& o, const char* name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, o, name))
            {
                return jsonGetVec3(outVec3, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetBool(bool& outBool, const json& o, const char* name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, o, name))
            {
                return jsonGetBool(outBool, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetVec4(glm::vec4& outVec3, const json& o, const char* name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, o, name))
            {
                return jsonGetVec4(outVec3, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetInt(i32& outInt, const json& o, const char* name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, o, name))
            {
                return jsonGetInt(outInt, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetUint(u32& outUint, const json& o, const char* name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, o, name))
            {
                return jsonGetUint(outUint, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetString(std::string& outStr, const json& o, const char* name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, o, name))
            {
                return jsonGetString(outStr, itr.value());
            }
            return false;
        }

        bool jsonFindAndGetFloat(f32& outFloat, const json& o, const char* name)
        {
            json_const_iterator itr;
            if (jsonFind(itr, o, name))
            {
                return jsonGetFloat(outFloat, itr.value());
            }
            return false;
        }

        static void from_json(const json& o, Scene& scene)
        {
            jsonFindAndGetString(scene.name, o, "name");
            json jNodes;
            if (jsonFind(jNodes, o, "nodes"))
            {
                if (jNodes.is_array())
                {
                    jNodes.get_to(scene.nodes);
                }
            }
        }

        static void from_json(const json& o, Node& node)
        {
            jsonFindAndGetString(node.name, o, "name");
            u32 mesh;
            if (jsonFindAndGetUint(mesh, o, "mesh"))
            {
                node.mesh = (i32)mesh;
            }
            json matrix;
            if (jsonFind(matrix, o, "matrix"))
            {
                node.hasMatrix = 1;
                matrix.get_to(node.matrix);
            }
            json scale;
            if (jsonFind(scale, o, "scale"))
            {
                node.hasScale = 1;
                scale.get_to(node.scale);
            }
            json rotation;
            if (jsonFind(rotation, o, "rotation"))
            {
                node.hasRotation = 1;
                rotation.get_to(node.rotation);
            }
            json translation;
            if (jsonFind(translation, o, "translation"))
            {
                node.hasTranslation = 1;
                translation.get_to(node.translation);
            }
        }

        static void from_json(const json& o, Buffer& buffer)
        {
            // required
            jsonFindAndGetUint(buffer.byteLength, o, "byteLength");
            // optional
            jsonFindAndGetString(buffer.name, o, "name");
            jsonFindAndGetString(buffer.uri, o, "uri");
        }

        static void from_json(const json& o, BufferView& bufferView)
        {
            // required
            jsonFindAndGetUint(bufferView.buffer, o, "buffer");
            jsonFindAndGetUint(bufferView.byteLength, o, "byteLength");
            // optional
            jsonFindAndGetUint(bufferView.byteOffset, o, "byteOffset");
            jsonFindAndGetUint(bufferView.byteStride, o, "byteStride");
            u32 target;
            if (jsonFindAndGetUint(target, o, "target"))
            {
                bufferView.target = target;
            }
            jsonFindAndGetString(bufferView.name, o, "name");
        }

        static void from_json(const json& o, Accessor& accessor)
        {
            // required
            jsonFindAndGetUint(accessor.componentType, o, "componentType");
            jsonFindAndGetUint(accessor.count, o, "count");
            jsonFindAndGetString(accessor.type, o, "type");
            // optional
            u32 bufferView;
            if (jsonFindAndGetUint(bufferView, o, "bufferView"))
            {
                accessor.bufferView = bufferView;
            }
            jsonFindAndGetUint(accessor.byteOffset, o, "byteOffset");
            jsonFindAndGetString(accessor.name, o, "name");
            jsonFindAndGetBool(accessor.normalized, o, "normalized");
        }

        static void from_json(const json& o, Attribute& attribute)
        {
            if (!jsonFindAndGetUint(attribute.position, o, "POSITION"))
            {
                assert(0);
            }
            if (!jsonFindAndGetUint(attribute.normal, o, "NORMAL"))
            {
                assert(0);
            }
            u32 tangent;
            if (jsonFindAndGetUint(tangent, o, "TANGENT"))
            {
                attribute.tangent = (i32)tangent;
            }
            u32 texCoord0;
            if (jsonFindAndGetUint(texCoord0, o, "TEXCOORD_0"))
            {
                attribute.texCoord0 = (i32)texCoord0;
            }
            u32 texCoord1;
            if (jsonFindAndGetUint(texCoord1, o, "TEXCOORD_1"))
            {
                attribute.texCoord1 = (i32)texCoord1;
            }
        }

        static void from_json(const json& o, Primitive& primitive)
        {
            // required
            json_const_iterator itr;
            if (!jsonFind(itr, o, "attributes"))
            {
                assert(0);
            }
            itr.value().get_to(primitive.attribute);
            // optional
            u32 indices;
            if (jsonFindAndGetUint(indices, o, "indices"))
            {
                primitive.indices = (i32)indices;
            }
            u32 material;
            if (jsonFindAndGetUint(material, o, "material"))
            {
                primitive.material = (i32)material;
            }
            u32 mode;
            if (jsonFindAndGetUint(mode, o, "mode"))
            {
                primitive.mode = (i32)mode;
            }
        }

        static void from_json(const json& o, Mesh& mesh)
        {
            // required
            json jPrimitives;
            jsonFind(jPrimitives, o, "primitives");
            if (jPrimitives.is_array())
            {
                u32 numPrimitives = jPrimitives.size();
                mesh.primitives.resize(numPrimitives);
                for (i32 i = 0; i < numPrimitives; ++i)
                {
                    jPrimitives[i].get_to(mesh.primitives[i]);
                }
            }
            // optional
            jsonFindAndGetString(mesh.name, o, "name");
        }

        static void from_json(const json& o, Image& image)
        {
            u32 bufferView;
            if (jsonFindAndGetUint(bufferView, o, "bufferView"))
            {
                image.bufferView = (i32)bufferView;
                if (!jsonFindAndGetString(image.mimeType, o, "mimeType"))
                {
                    // todo: issue error
                }
            }
            else
            {
                if (!jsonFindAndGetString(image.uri, o, "uri"))
                {
                    // todo: issue error
                }
            }
            jsonFindAndGetString(image.name, o, "name");
        }

        static void from_json(const json& o, Sampler& sampler)
        {
            jsonFindAndGetUint(sampler.magFilter, o, "magFilter");
            jsonFindAndGetUint(sampler.minFilter, o, "minFilter");
            jsonFindAndGetUint(sampler.wrapS, o, "wrapS");
            jsonFindAndGetUint(sampler.wrapT, o, "wrapT");
            jsonFindAndGetString(sampler.name, o, "name");
        }

        static void from_json(const json& o, Texture& texture)
        {
            u32 source;
            if (jsonFindAndGetUint(source, o, "source"))
            {
                texture.source = (i32)source;
            }
            u32 sampler;
            if (jsonFindAndGetUint(sampler, o, "sampler"))
            {
                texture.sampler = sampler;
            }
            jsonFindAndGetString(texture.name, o, "name");
        }

        static void from_json(const json& o, TextureInfo& textureInfo)
        {
            u32 index;
            if (jsonFindAndGetUint(index, o, "index"))
            {
                textureInfo.index = (i32)index;
            }
            jsonFindAndGetUint(textureInfo.texCoord, o, "texCoord");
        }

        static void from_json(const json& o, PbrMetallicRoughness& pbrMetallicRoughness)
        {
            json jBaseColorTexture;
            if (jsonFind(jBaseColorTexture, o, "baseColorTexture"))
            {
                jBaseColorTexture.get_to(pbrMetallicRoughness.baseColorTexture);
            }
            json jMetallicRoughness;
            if (jsonFind(jMetallicRoughness, o, "metallicRoughnessTexture"))
            {
                jMetallicRoughness.get_to(pbrMetallicRoughness.metallicRoughnessTexture);
            }
            jsonFindAndGetVec4(pbrMetallicRoughness.baseColorFactor, o, "baseColorFactor");
            jsonFindAndGetFloat(pbrMetallicRoughness.metallicFactor, o, "metallicFactor");
            jsonFindAndGetFloat(pbrMetallicRoughness.roughnessFactor, o, "roughnessFactor");
        }

        static void from_json(const json& o, Material& material)
        {
            jsonFindAndGetString(material.name, o, "name");
            json jPbrMetallicRoughness;
            if (jsonFind(jPbrMetallicRoughness, o, "pbrMetallicRoughness"))
            {
                jPbrMetallicRoughness.get_to(material.pbrMetallicRoughness);
            }
            json jNormalTexture;
            if (jsonFind(jNormalTexture, o, "normalTexture"))
            {
                jNormalTexture.get_to(material.normalTexture);
            }
            json jOcclusionTexture;
            if (jsonFind(jOcclusionTexture, o, "occlusionTexture"))
            {
                jOcclusionTexture.get_to(material.occlusionTexture);
            }
            json jEmissiveTexture;
            if (jsonFind(jEmissiveTexture, o, "emissiveTexture"))
            {
                jEmissiveTexture.get_to(material.emissiveTexture);
            }
            // todo: some fields are not loaded
        }
    }
}