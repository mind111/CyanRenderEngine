#pragma once

#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#include "stb_image.h"
#include "tiny_gltf.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Common.h"
#include "Texture.h"
#include "Scene.h"
#include "Mesh.h"
#include "CyanAPI.h"

struct Asset
{

};

// TODO: improve readability

class AssetManager
{
public:

    // TODO: Fix this, the loaded textures are all black!!!
    Cyan::Texture* loadGltfTexture(tinygltf::Model& model, i32 index) {
        using Cyan::Texture;

        Texture* texture = nullptr;
        if (index > -1) {
            auto gltfTexture = model.textures[index];
            auto image = model.images[gltfTexture.source];
            u32 sizeInBytes = image.image.size();
            Cyan::TextureSpec spec = { };
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_width = image.width;
            spec.m_height = image.height;
            switch (image.component) {
                case 3: {
                    if (image.bits == 8) {
                        spec.m_format = Cyan::Texture::ColorFormat::R8G8B8;
                    } else if (image.bits == 16) {
                        spec.m_format = Cyan::Texture::ColorFormat::R16G16B16;
                    } else if (image.bits == 32) {
                        CYAN_ASSERT(0, "R32G32B32 color format is currently not supported!")
                    } 
                    break;
                }
                case 4: {
                    if (image.bits == 8) {
                        spec.m_format = Cyan::Texture::ColorFormat::R8G8B8A8;
                    } else if (image.bits == 16) {
                        spec.m_format = Cyan::Texture::ColorFormat::R16G16B16A16;
                    } else if (image.bits == 32) {
                        CYAN_ASSERT(0, "R32G32B32A32 color format is currently not supported!")
                    } 
                    break;
                }
                default: {
                    break;
                }
            }
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_numMips = 11;
            spec.m_data = reinterpret_cast<void*>(image.image.data());
            char name[64];
            sprintf(name, "%s", image.uri.c_str());
            switch (spec.m_format) {
                case Texture::ColorFormat::R8G8B8:
                case Texture::ColorFormat::R8G8B8A8: {
                    texture = Cyan::createTexture(name, spec);
                    break;
                }
                case Texture::ColorFormat::R16G16B16:
                case Texture::ColorFormat::R16G16B16A16: {
                    texture = Cyan::createTextureHDR(name, spec);
                    break;
                }
            }
            Cyan::addTexture(texture);
        }
        return texture;
    }

    // TODO: Normalize mesh scale
    void loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, 
                        SceneNode* parentSceneNode, tinygltf::Node& node, u32 numNodes) {
        bool hasMesh = (node.mesh > -1);
        bool hasSkin = (node.skin > -1);
        bool hasMatrix = !node.matrix.empty();
        bool hasTransformComponent = (!node.rotation.empty() || !node.translation.empty() ||
                                    !node.scale.empty());
        bool hasTransform = (hasMatrix || hasTransformComponent);
        const char* meshName = hasMesh ? model.meshes[node.mesh].name.c_str() : nullptr;
        Transform localTransform;
        if (hasMatrix) {
            glm::mat4 matrix = {
                glm::vec4(node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3]),     // column 0
                glm::vec4(node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7]),     // column 1
                glm::vec4(node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11]),   // column 2
                glm::vec4(node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15])  // column 3
            };
            // convert matrix to local transform
            localTransform.fromMatrix(matrix);
        } else if (hasTransformComponent) {
            if (!node.translation.empty()) {
                localTransform.m_translate = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }
            if (!node.scale.empty()) {
                localTransform.m_scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }
            if (!node.rotation.empty()) {
                // glm quaternion constructor (w, x, y, z) while gltf (x, y, z, w)
                localTransform.m_qRot = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            }
        }

        // create an entity for this node
        char entityName[64];
        CYAN_ASSERT(node.name.size() < kEntityNameMaxLen, "Entity name too long !!")
        if (node.name.empty()) {
            sprintf_s(entityName, "Node%u", numNodes);
        } else {
            sprintf_s(entityName, "%s", node.name.c_str());
        }
        // every node has a transform component
        Entity* entity = SceneManager::createEntity(scene, entityName, meshName, localTransform, true);
        SceneNode* sceneNode = SceneManager::createSceneNode(scene, parentSceneNode, entity);
        // bind material
        if (entity->m_meshInstance) {
            auto& gltfMesh = model.meshes[node.mesh];
            for (u32 sm = 0u; sm < gltfMesh.primitives.size(); ++sm) {
                // !TODO: material is hard coded to pbr material for now 
                Shader* pbrShader = Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs", "../../shader/shader_pbr.fs");
                Cyan::MeshInstance* mesh = entity->m_meshInstance;
                mesh->m_matls[sm] = Cyan::createMaterial(pbrShader)->createInstance();
                auto& primitive = gltfMesh.primitives[sm];
                if (primitive.material > -1) {
                    auto& gltfMaterial = model.materials[primitive.material];
                    auto pbr = gltfMaterial.pbrMetallicRoughness;
                    auto getTexture = [&](i32 imageIndex) {
                        Cyan::Texture* texture = nullptr;
                        if (imageIndex > -1) {
                            auto& image = model.images[imageIndex];
                            texture = Cyan::getTexture(image.uri.c_str());
                        }
                        return texture;
                    };

                    // albedo
                    Cyan::Texture* albedo = getTexture(pbr.baseColorTexture.index);
                    mesh->m_matls[sm]->bindTexture("diffuseMaps[0]", albedo); 
                    // normal map
                    Cyan::Texture* normal = getTexture(gltfMaterial.normalTexture.index);
                    mesh->m_matls[sm]->bindTexture("normalMap", normal); 
                    // metallicRoughness
                    Cyan::Texture* metallicRoughness = getTexture(pbr.metallicRoughnessTexture.index);
                    mesh->m_matls[sm]->bindTexture("roughnessMap", metallicRoughness); 
                    // occlusion
                    Cyan::Texture* occlusion = getTexture(gltfMaterial.occlusionTexture.index);
                    mesh->m_matls[sm]->bindTexture("aoMap", occlusion); 

                    mesh->m_matls[sm]->bindBuffer("dirLightsData", scene->m_dirLightsBuffer);
                    mesh->m_matls[sm]->bindBuffer("pointLightsData", scene->m_pointLightsBuffer);
                    mesh->m_matls[sm]->set("debugG", 0.f);
                    mesh->m_matls[sm]->set("debugF", 0.f);
                    mesh->m_matls[sm]->set("debugD", 0.f);
                    mesh->m_matls[sm]->set("disneyReparam", 1.f);
                    mesh->m_matls[sm]->set("hasAoMap", 1.f);
                    mesh->m_matls[sm]->set("hasNormalMap", 1.f);
                    mesh->m_matls[sm]->set("kDiffuse", 1.0f);
                    mesh->m_matls[sm]->set("kSpecular", 1.0f);
                    mesh->m_matls[sm]->set("hasRoughnessMap", 0.f);
                    mesh->m_matls[sm]->set("hasMetallicRoughnessMap", 1.f);
                    mesh->m_matls[sm]->set("directDiffuseSlider", 1.0f);
                    mesh->m_matls[sm]->set("directSpecularSlider", 1.0f);
                    mesh->m_matls[sm]->set("indirectDiffuseSlider", 1.0f);
                    mesh->m_matls[sm]->set("indirectSpecularSlider", 1.0f);
                    mesh->m_matls[sm]->set("wrap", 0.15f);
                }
            }
        }
        // recurse to load all the children
        for (auto& child : node.children) {
            loadGltfNode(scene, model, &node, sceneNode, model.nodes[child], ++numNodes);
        }
    }

    Cyan::Mesh* loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh) {
        using Cyan::Mesh;
        Mesh* mesh = new Mesh;
        mesh->m_name = gltfMesh.name;

        // primitives (submeshes)
        for (u32 i = 0u; i < (u32)gltfMesh.primitives.size(); ++i)
        {
            Mesh::SubMesh* subMesh = new Mesh::SubMesh;
            tinygltf::Primitive primitive = gltfMesh.primitives[i];
            u32 numVertices = 0u, numIndices = 0u;
            // Convert data for each vertex attributes into raw buffer
            u32 strideInBytes = 0u;
            auto incrementVertexStride = [&](auto& attribute) {
                tinygltf::Accessor accessor = model.accessors[attribute.second];
                if (numVertices > 0u) {
                    CYAN_ASSERT(numVertices == accessor.count, "Mismatch vertex count among vertex attributes")
                } else {
                    numVertices = accessor.count;
                }
                strideInBytes += tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
            };
            struct SortAttribute
            {
                std::string name;
                u32 key;
                bool operator<(SortAttribute& rhs)
                {
                    return key < rhs.key;
                }
            };
            std::vector<SortAttribute> sortedAttribs; 
            for (auto& attrib : primitive.attributes)
            {
                if (attrib.first.compare("POSITION") == 0) {
                    incrementVertexStride(attrib);
                    sortedAttribs.push_back({attrib.first, 0u});
                } else if (attrib.first.compare("NORMAL") == 0) {
                    incrementVertexStride(attrib);
                    sortedAttribs.push_back({attrib.first, 1u});
                } else if (attrib.first.compare("TANGENT") == 0) {
                    incrementVertexStride(attrib);
                    sortedAttribs.push_back({attrib.first, 2u});
                } else if (attrib.first.find("TEXCOORD") == 0) {
                    incrementVertexStride(attrib);
                    u32 texCoordIndex = 0u;
                    sscanf_s(attrib.first.c_str(),"TEXCOORD_%u", &texCoordIndex);
                    sortedAttribs.push_back({attrib.first, texCoordIndex + 3u});
                }
            }
            // sort the attributes in an order that is compatible with the bindings order in 
            // shader_pbr
            std::sort(sortedAttribs.begin(), sortedAttribs.end());
            f32* vertexDataBuffer = reinterpret_cast<f32*>(new u8[strideInBytes * numVertices]); 
            u32 totalBytes = 0u;
            u32 offset = 0u;
            std::vector<VertexAttrib> vertexAttribs;
            // vertices
            // reorganize the attribute data to interleave them in the buffer
            for (auto& entry : sortedAttribs)
            {
                auto& attrib = primitive.attributes.find(entry.name);
                tinygltf::Accessor accessor = model.accessors[attrib->second];
                tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
                u8* srcStart = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                u8* dstStart = reinterpret_cast<u8*>(vertexDataBuffer) + offset;
                u32 sizeToCopy = tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
                for (u32 v = 0; v < numVertices; ++v)
                {
                    void* srcDataAddress = reinterpret_cast<void*>(srcStart + v * bufferView.byteStride);
                    void* dstDataAddress = reinterpret_cast<void*>(dstStart + v * strideInBytes); 
                    memcpy(dstDataAddress, srcDataAddress, sizeToCopy);
                    // TODO: Do this in a not so hacky way 
                    // flip the y-component of texcoord
                    if (entry.name.find("TEXCOORD") == 0) {
                        float* data = reinterpret_cast<float*>(dstDataAddress);
                        data[1] = 1.f - data[1];
                    }
                    totalBytes += sizeToCopy;
                }
                // FIXME: type is hard-coded fo float for now
                vertexAttribs.push_back({
                    VertexAttrib::DataType::Float,
                    static_cast<u32>(tinygltf::GetNumComponentsInType(accessor.type)),
                    strideInBytes,
                    offset,
                    nullptr
                });
                offset += sizeToCopy;
            }
            CYAN_ASSERT(totalBytes == strideInBytes * numVertices, "mismatched buffer size")
            VertexBuffer* vb = Cyan::createVertexBuffer(reinterpret_cast<void*>(vertexDataBuffer), strideInBytes * numVertices, strideInBytes, numVertices);
            for (auto& va : vb->m_vertexAttribs) {
                va.m_data = reinterpret_cast<void*>(reinterpret_cast<u8*>(vb->m_data) + va.m_offset);
            }
            vb->m_vertexAttribs = vertexAttribs;
            subMesh->m_vertexArray = Cyan::createVertexArray(vb);
            subMesh->m_vertexArray->init();
            subMesh->m_numVerts = numVertices;
            // indices
            if (primitive.indices >= 0)
            {
                auto& accessor = model.accessors[primitive.indices];
                numIndices = accessor.count;
                tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
                u32 indexSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
                // FIXME: type is hard-coded fo u32 for now
                u32* indexDataBuffer  = new u32[numIndices];
                void* srcDataAddress = reinterpret_cast<void*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                memcpy(indexDataBuffer, srcDataAddress, numIndices * indexSize);
                glCreateBuffers(1, &subMesh->m_vertexArray->m_ibo);
                glBindVertexArray(subMesh->m_vertexArray->m_vao);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh->m_vertexArray->m_ibo);
                glNamedBufferData(subMesh->m_vertexArray->m_ibo, numIndices * indexSize,
                                    reinterpret_cast<const void*>(indexDataBuffer), GL_STATIC_DRAW);
                glBindVertexArray(0);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                subMesh->m_vertexArray->m_numIndices = numIndices;
                delete[] vertexDataBuffer;
                delete[] indexDataBuffer;
            }
            mesh->m_subMeshes.push_back(subMesh);
        } // primitive (submesh)
        // TODO: This is not correct
        // mesh->m_normalization = Cyan::Toolkit::computeMeshNormalization(mesh);
        mesh->m_normalization = glm::scale(glm::mat4(1.f), glm::vec3(0.01f, 0.01f, 0.01f));
        return mesh;
    }

    void loadGltfTextures(tinygltf::Model& model) {
        using Cyan::Texture;
        for (u32 t = 0u; t < model.textures.size(); ++t) {
            loadGltfTexture(model, t);
        }
    }

    void loadGltf(Scene* scene, const char* filename)
    {
        using Cyan::Mesh;
        tinygltf::Model model;
        std::string warn, err;
        if (!m_loader.LoadASCIIFromFile(&model, &err, &warn, std::string(filename)))
        {
            std::cout << warn << std::endl;
            std::cout << err << std::endl;
        }
        tinygltf::Scene& gltfScene = model.scenes[model.defaultScene];
        // load textures
        loadGltfTextures(model);
        // load meshes
        for (auto& gltfMesh : model.meshes)
        {
            Mesh* mesh = loadGltfMesh(model, gltfMesh);
            Cyan::addMesh(mesh);
        }
        // FIXME: Handle multiple root nodes
        // assuming that there is only one root node for defaultScene
        tinygltf::Node rootNode = model.nodes[gltfScene.nodes[0]];
        loadGltfNode(scene, model, nullptr, scene->m_root, rootNode, 0);
    }

    tinygltf::TinyGLTF m_loader;
    Assimp::Importer m_importer;
};