#pragma once

#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#include "tiny_gltf.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Common.h"
#include "Scene.h"
#include "Mesh.h"
#include "CyanAPI.h"

struct Asset
{

};

class AssetManager
{
public:

    void interleaveVertexAttribute(float* dstBuffer, tinygltf::Buffer& srcBuffer, u32 numVertices)
    {
        for (u32 v = 0u; v < numVertices; ++v)
        {

        }
    }

    // FIXME: Some parts of the mesh is not visable
    // FIXME: Verify transform hierachy
    // FIXME: Normalize mesh scale
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
                localTransform.m_qRot = glm::quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]);
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
                delete[] indexDataBuffer;
            }
            // FIXME: load material for current submesh
            // material
            tinygltf::Material gltfMaterial = model.materials[primitive.material];
            mesh->m_subMeshes.push_back(subMesh);
        } // primitive (submesh)
        // FIXME: This is not correct
        mesh->m_normalization = Cyan::Toolkit::computeMeshNormalization(mesh);
        return mesh;
    }

    void loadGltfTextures() {

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
        for (auto& image : model.images) {
            continue;
        }
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