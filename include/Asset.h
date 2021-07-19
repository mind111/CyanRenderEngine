#pragma once

#include <iostream>
#include <string>

#include "tiny_gltf.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Common.h"
#include "Mesh.h"
#include "CyanAPI.h"

struct Asset
{

};

class AssetManager
{
public:
    void loadGltf(Cyan::Mesh* mesh, const char* filename)
    {
        using Cyan::Mesh;

        tinygltf::Model model;
        std::string warn, err;
        if (!m_loader.LoadASCIIFromFile(&model, &err, &warn, std::string(filename)))
        {
            std::cout << warn << std::endl;
            std::cout << err << std::endl;
        }
        tinygltf::Scene& scene = model.scenes[model.defaultScene];
        // FIXME: Handle multiple root nodes
        // assuming that there is only one root node for defaultScene
        tinygltf::Node rootNode = model.nodes[scene.nodes[0]];
        // FIXME: Technically, a gltf file can contain multiple meshes, and it doesn't define
        // parent-child relationship for each mesh. I would like to organize all the submeshes
        // within one Cyan::Mesh instance instead of treating each entry in model.meshes as an
        // independent Cyan::Mesh
        // FIXME: For now, just assume that all the meshes contained in a gltf can be viewed as
        // submeshes

        // store all the mesh data
        for (auto& gltfMesh : model.meshes)
        {
            Mesh::SubMesh* subMesh = new Mesh::SubMesh;
            // primitives (Is this equivalent to subMeshes)
            for (u32 i = 0u; i < (u32)gltfMesh.primitives.size(); ++i)
            {
                tinygltf::Primitive primitive = gltfMesh.primitives[i];
                u32 numVertices = 0u, numIndices = 0u;
                if (primitive.indices >= 0)
                {
                    auto& accessor = model.accessors[primitive.indices];
                    numIndices = accessor.count;
                }
                // Convert data for each vertex attributes into raw buffer
                u32 strideInBytes = 0u;
                auto incrementVertexStride = [&](auto& attribute) {
                    tinygltf::Accessor accessor = model.accessors[attribute.second];
                    if (numVertices > 0u) {
                        CYAN_ASSERT(numVertices == accessor.count, "Mismatch vertex count among vertex attributes")
                    } else {
                        numVertices = accessor.count;
                    }
                    strideInBytes += accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                };
                for (auto& attrib : primitive.attributes)
                {
                    if (attrib.first.compare("POSITION") == 0) {
                        incrementVertexStride(attrib);
                    } else if (attrib.first.compare("NORMAL")) {
                        incrementVertexStride(attrib);
                    } else if (attrib.first.compare("TANGENT")) {
                        incrementVertexStride(attrib);
                    } else if (attrib.first.find("TEXCOORD")) {
                        incrementVertexStride(attrib);
                    }
                    tinygltf::Accessor accessor = model.accessors[attrib.second];
                    numVertices = accessor.count;
                    strideInBytes += accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                }
                u32 totalBytes = 0u;
                float* vertexDataBuffer = (float*)new u8[strideInBytes * numVertices]; 
                u16*   indexDataBuffer  = new u16[numIndices];
                for (auto& attrib : primitive.attributes)
                {
                    tinygltf::Accessor accessor = model.accessors[attrib.second];
                    tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                    tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
                    memcpy((void*)BytesOffset(vertexDataBuffer, totalBytes), buffer.data.data(), buffer.data.size());
                    totalBytes += buffer.data.size();
                }
                VertexBuffer* vb = Cyan::createVertexBuffer(reinterpret_cast<void*>(vertexDataBuffer), totalBytes, strideInBytes, numVertices);
                vb->m_vertexAttribs.push_back();
            }
            mesh->m_subMeshes.push_back(subMesh);
        }

        // figure out the hierachy between all the nodes
        if (!rootNode.matrix.empty())
        {

        }
        else
        {

        }
    }

    tinygltf::TinyGLTF m_loader;
    Assimp::Importer m_importer;
};