#include <fstream>

#define GLM_ENABLE_EXPERIMENTAL
#include "gtx/hash.hpp"
#include "tiny_obj_loader.h"
#include "xatlas.h"

#include "Asset.h"
#include "Texture.h"
#include "CyanAPI.h"

bool operator==(const Cyan::Vertex& lhs, const Cyan::Vertex& rhs)
{
    return (lhs.position == rhs.position) 
        && (lhs.normal == rhs.normal) 
        && (lhs.texCoord == rhs.texCoord);
}

namespace std {
    template<> 
    struct hash<Cyan::Vertex> 
    {
        size_t operator()(Cyan::Vertex const& vertex) const 
        {
            size_t a = hash<glm::vec3>()(vertex.position); 
            size_t b = hash<glm::vec3>()(vertex.normal); 
            size_t c = hash<glm::vec2>()(vertex.texCoord); 
            return (a ^ (b << 1)) ^ (c << 1);
        }
    };
}

namespace glm {
    //---- Utilities for loading the scene data from json
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

void from_json(const nlohmann::json& j, Transform& t) 
{
    t.m_translate = j.at("translation").get<glm::vec3>();
    glm::vec4 rotation = j.at("rotation").get<glm::vec4>();
    t.m_qRot = glm::quat(cos(RADIANS(rotation.x * 0.5f)), sin(RADIANS(rotation.x * 0.5f)) * glm::vec3(rotation.y, rotation.z, rotation.w));
    t.m_scale = j.at("scale").get<glm::vec3>();
}

void from_json(const nlohmann::json& j, Camera& c) 
{
    c.position = j.at("position").get<glm::vec3>();
    c.lookAt = j.at("lookAt").get<glm::vec3>();
    c.worldUp = j.at("worldUp").get<glm::vec3>();
    j.at("fov").get_to(c.fov);
    j.at("z_far").get_to(c.f);
    j.at("z_near").get_to(c.n);
}

namespace Cyan
{

    AssetManager* AssetManager::singletonPtr = nullptr;
    AssetManager::AssetManager()
    {
        if (!singletonPtr)
        {
            singletonPtr = this;
        }
    }

    void AssetManager::loadTextures(nlohmann::basic_json<std::map>& textureInfoList)
    {
        using Cyan::Texture;
        using Cyan::TextureSpec;
        auto textureManager = Cyan::TextureManager::getSingletonPtr();

        for (auto textureInfo : textureInfoList) 
        {
            std::string filename = textureInfo.at("path").get<std::string>();
            std::string name     = textureInfo.at("name").get<std::string>();
            std::string dynamicRange = textureInfo.at("dynamic_range").get<std::string>();
            u32 numMips = textureInfo.at("numMips").get<u32>();
            
            TextureSpec spec = { };
            spec.type = Texture::Type::TEX_2D;
            spec.numMips = numMips;
            spec.min = Texture::Filter::LINEAR;
            spec.mag = Texture::Filter::LINEAR;
            spec.s = Texture::Wrap::NONE;
            spec.t = Texture::Wrap::NONE;
            spec.r = Texture::Wrap::NONE;
            Texture* texture = nullptr;
            if (dynamicRange == "ldr")
            {
                texture = textureManager->createTexture(name.c_str(), filename.c_str(), spec);
            }
            else if (dynamicRange == "hdr")
            {
                texture = textureManager->createTextureHDR(name.c_str(), filename.c_str(), spec);
            }
            m_textureMap.insert(std::string(name), texture);
        }
    }

    /*
    https://github.com/jpcy/xatlas/blob/master/source/examples/example.cpp

    MIT License
    Copyright (c) 2018-2020 Jonathan Young
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
    */
    static int Print(const char *format, ...)
    {
        va_list arg;
        va_start(arg, format);
        printf("\r"); // Clear progress text.
        const int result = vprintf(format, arg);
        va_end(arg);
        return result;
    }

    void addSubMeshToLightMap(xatlas::Atlas* atlas, std::vector<Vertex>& vertices, std::vector<u32>& indices)
    {
        xatlas::SetPrint(Print, true);

        // Add meshes to atlas.
        xatlas::MeshDecl meshDecl;
        meshDecl.vertexCount = vertices.size();
        meshDecl.vertexPositionData = &vertices[0].position.x;
        meshDecl.vertexPositionStride = sizeof(Vertex);
        meshDecl.vertexNormalData = &vertices[0].normal.x;
        meshDecl.vertexNormalStride = sizeof(Vertex);
        meshDecl.vertexUvData = &vertices[0].texCoord.x;
        meshDecl.vertexUvStride = sizeof(Vertex);
        meshDecl.indexCount = (u32)indices.size();
        meshDecl.indexData = indices.data();
        meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);
        if (error != xatlas::AddMeshError::Success) 
            cyanError("Error adding mesh");
    }

    void computeTangent(std::vector<Vertex>& vertices, u32 face[3])
    {
        auto& v0 = vertices[face[0]];
        auto& v1 = vertices[face[1]];
        auto& v2 = vertices[face[2]];
        glm::vec3 v0v1 = v1.position - v0.position;
        glm::vec3 v0v2 = v2.position - v0.position;
        glm::vec2 deltaUv0 = v1.texCoord - v0.texCoord;
        glm::vec2 deltaUv1 = v2.texCoord - v0.texCoord;
        f32 tx = (deltaUv1.y * v0v1.x - deltaUv0.y * v0v2.x) / (deltaUv0.x * deltaUv1.y - deltaUv1.x * deltaUv0.y);
        f32 ty = (deltaUv1.y * v0v1.y - deltaUv0.y * v0v2.y) / (deltaUv0.x * deltaUv1.y - deltaUv1.x * deltaUv0.y);
        f32 tz = (deltaUv1.y * v0v1.z - deltaUv0.y * v0v2.z) / (deltaUv0.x * deltaUv1.y - deltaUv1.x * deltaUv0.y);
        glm::vec3 tangent(tx, ty, tz);
        tangent = glm::normalize(tangent);
        v0.tangent = glm::vec4(tangent, 1.f);
        v1.tangent = glm::vec4(tangent, 1.f);
        v2.tangent = glm::vec4(tangent, 1.f);
    }

    void loadObjTriMesh()
    {

    }

    void loadObjLineMesh()
    {

    }

    // treat all the meshes inside one obj file as submeshes
    Mesh* AssetManager::loadObj(const char* baseDir, const char* filename, bool bGenerateLightMapUv)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, baseDir);
        if (!ret)
        {
            cyanError("Failed loading obj file %s ", filename);
            cyanError("Warnings: %s               ", warn.c_str());
            cyanError("Errors:   %s               ", err.c_str());
        }

        Mesh* mesh = new Mesh;
        for (u32 i = 0; i < materials.size(); ++i)
        {
            mesh->m_objMaterials.emplace_back();
            auto& objMatl = mesh->m_objMaterials.back();
            objMatl.diffuse = glm::vec3{ materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2] };
            objMatl.specular = glm::vec3{ materials[i].specular[0], materials[i].specular[2], materials[i].specular[3] };
            objMatl.kMetalness = materials[i].metallic;
            objMatl.kRoughness = materials[i].roughness;
        }

        std::vector<TriMesh*> objMeshes;
        // submeshes
        for (u32 s = 0; s < shapes.size(); ++s)
        {
#if CYAN_DEBUG
            cyanInfo("shape[%d].name = %s", s, shapes[s].name.c_str());
#endif
            TriMesh* objMesh = new TriMesh;
            objMeshes.push_back(objMesh);
            Mesh::SubMesh* subMesh = new Mesh::SubMesh;
            mesh->m_subMeshes.push_back(subMesh);

            // load tri mesh
            if (shapes[s].mesh.indices.size() > 0)
            {
                std::vector<Vertex>& vertices = objMesh->vertices;
                std::vector<u32>& indices = objMesh->indices;
                indices.resize(shapes[s].mesh.indices.size());
                std::unordered_map<Vertex, u32> uniqueVerticesMap;
                u32 numUniqueVertices = 0;

                // assume that one submesh can only have one material
                if (shapes[s].mesh.material_ids.size() > 0)
                {
                    subMesh->m_materialIdx = shapes[s].mesh.material_ids[0];
                }
                // load triangles
                for (u32 f = 0; f < shapes[s].mesh.indices.size() / 3; ++f)
                {
                    Vertex vertex = { };
                    u32 face[3] = { };

                    for (u32 v = 0; v < 3; ++v)
                    {
                        tinyobj::index_t index = shapes[s].mesh.indices[f * 3 + v];
                        // position
                        f32 vx = attrib.vertices[index.vertex_index * 3 + 0];
                        f32 vy = attrib.vertices[index.vertex_index * 3 + 1];
                        f32 vz = attrib.vertices[index.vertex_index * 3 + 2];
                        vertex.position = glm::vec3(vx, vy, vz);
                        // normal
                        if (index.normal_index >= 0)
                        {
                            f32 nx = attrib.normals[index.normal_index * 3 + 0];
                            f32 ny = attrib.normals[index.normal_index * 3 + 1];
                            f32 nz = attrib.normals[index.normal_index * 3 + 2];
                            vertex.normal = glm::vec3(nx, ny, nz);
                        }
                        else
                            vertex.normal = glm::vec3(0.f);
                        // texcoord
                        if (index.texcoord_index >= 0)
                        {
                            f32 tx = attrib.texcoords[index.texcoord_index * 2 + 0];
                            f32 ty = attrib.texcoords[index.texcoord_index * 2 + 1];
                            vertex.texCoord = glm::vec2(tx, ty);
                        }
                        else
                            vertex.texCoord = glm::vec2(0.f);

                        // deduplicate vertices
                        auto iter = uniqueVerticesMap.find(vertex);
                        if (iter == uniqueVerticesMap.end())
                        {
                            uniqueVerticesMap[vertex] = numUniqueVertices;
                            vertices.push_back(vertex);
                            face[v] = numUniqueVertices;
                            indices[f * 3 + v] = numUniqueVertices++;
                        }
                        else
                        {
                            u32 reuseIndex = iter->second;
                            indices[f * 3 + v] = reuseIndex;
                            face[v] = reuseIndex;
                        }

                        subMesh->m_triangles.m_positionArray.push_back(vertex.position);
                        subMesh->m_triangles.m_normalArray.push_back(vertex.normal);
                        subMesh->m_triangles.m_texCoordArray.push_back(glm::vec3(vertex.texCoord, 0.f));
                    }

                    // compute face tangent
                    computeTangent(vertices, face);
                }
            } 
            // todo: this loading code is extremely buggy!!! 
            // load lines
            if (shapes[s].lines.indices.size() > 0)
            {
                std::vector<Vertex>& vertices = objMesh->vertices;
                vertices.resize(shapes[s].lines.num_line_vertices.size());
                std::vector<u32>& indices = objMesh->indices;
                indices.resize(shapes[s].lines.indices.size());
                std::unordered_map<Vertex, u32> uniqueVerticesMap;
                u32 numUniqueVertices = 0;

                for (u32 l = 0; l < shapes[s].lines.indices.size() / 2; ++l)
                {
                    Vertex vertex = { };
                    for (u32 v = 0; v < 2; ++v)
                    {
                        tinyobj::index_t index = shapes[s].lines.indices[l * 2 + v];
                        f32 vx = attrib.vertices[index.vertex_index * 3 + 0];
                        f32 vy = attrib.vertices[index.vertex_index * 3 + 1];
                        f32 vz = attrib.vertices[index.vertex_index * 3 + 2];
                        vertex.position = glm::vec3(vx, vy, vz);
                        vertices[index.vertex_index] = vertex;
                        indices[l * 2 + v] = index.vertex_index;
#if 0
                        // deduplicate vertices
                        auto iter = uniqueVerticesMap.find(vertex);
                        if (iter == uniqueVerticesMap.end())
                        {
                            uniqueVerticesMap[vertex] = numUniqueVertices;
                            vertices.push_back(vertex);
                            indices[l * 2 + v] = numUniqueVertices++;
                        }
                        else
                        {
                            u32 reuseIndex = iter->second;
                            indices[l * 2 + v] = reuseIndex;
                        }
#endif
                    }
                }
            }
        }

        if (bGenerateLightMapUv)
        {
            auto atlas = xatlas::Create();
            for (auto objMesh : objMeshes)
                addSubMeshToLightMap(atlas, objMesh->vertices, objMesh->indices);
            // atlas now holds results of packing
            xatlas::PackOptions packOptions = { };
            packOptions.bruteForce = true;
            packOptions.padding = 5.f;
            packOptions.resolution = 1024;

            xatlas::Generate(atlas, xatlas::ChartOptions{}, packOptions);
            CYAN_ASSERT(atlas->meshCount == objMeshes.size(), "# Submeshes and # of meshes in atlas doesn't match!");
            mesh->m_lightMapWidth = atlas->width;
            mesh->m_lightMapHeight = atlas->height;

            for (u32 sm = 0; sm < objMeshes.size(); ++sm)
            {
                std::vector<Vertex> packedVertices(atlas->meshes[sm].vertexCount);
                std::vector<u32>       packedIndices(atlas->meshes[sm].indexCount);
                for (u32 v = 0; v < atlas->meshes[sm].vertexCount; ++v)
                {
                    xatlas::Vertex atlasVertex = atlas->meshes[sm].vertexArray[v];
                    packedVertices[v] = objMeshes[sm]->vertices[atlasVertex.xref];
                    packedVertices[v].texCoord1.x = atlasVertex.uv[0] / atlas->width;
                    packedVertices[v].texCoord1.y = atlasVertex.uv[1] / atlas->height;
                }
                for (u32 i = 0; i < atlas->meshes[sm].indexCount; ++i)
                    packedIndices[i] = atlas->meshes[sm].indexArray[i];
                objMeshes[sm]->vertices.swap(packedVertices);
                objMeshes[sm]->indices.swap(packedIndices);
            }
            xatlas::Destroy(atlas);
        }

        u32 strideInBytes = sizeof(Vertex);
        for (u32 sm = 0; sm < mesh->m_subMeshes.size(); ++sm)
        {
            TriMesh* objMesh = objMeshes[sm];
            auto vb = Cyan::createVertexBuffer(objMesh->vertices.data(), sizeof(Vertex) * objMesh->vertices.size(), strideInBytes, objMesh->indices.size());
            u32 offset = 0;
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 3, strideInBytes, offset});
            offset += 3 * sizeof(f32);
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 3, strideInBytes, offset});
            offset += 3 * sizeof(f32);
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 4, strideInBytes, offset});
            offset += 4 * sizeof(f32);
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 2, strideInBytes, offset});
            offset += 2 * sizeof(f32);
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 2, strideInBytes, offset});
            offset += 2 * sizeof(f32);
            mesh->m_subMeshes[sm]->m_vertexArray = Cyan::createVertexArray(vb);
            mesh->m_subMeshes[sm]->m_vertexArray->init();
            mesh->m_subMeshes[sm]->m_numVerts = objMesh->vertices.size();
            mesh->m_subMeshes[sm]->m_numIndices = objMesh->indices.size();
            mesh->m_subMeshes[sm]->m_triangles.m_numVerts = objMesh->indices.size();
            // create index buffer
            glCreateBuffers(1, &mesh->m_subMeshes[sm]->m_vertexArray->m_ibo);
            glBindVertexArray(mesh->m_subMeshes[sm]->m_vertexArray->m_vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->m_subMeshes[sm]->m_vertexArray->m_ibo);
            glNamedBufferData(mesh->m_subMeshes[sm]->m_vertexArray->m_ibo, objMesh->indices.size() * sizeof(u32), objMesh->indices.data(), GL_STATIC_DRAW);
            glBindVertexArray(0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            mesh->m_subMeshes[sm]->m_vertexArray->m_numIndices = objMesh->indices.size();
        }

        // release resources
        for (auto objMesh : objMeshes)
            delete objMesh;
        return mesh;
    }

    Mesh* AssetManager::loadMesh(std::string& path, const char* name, bool normalize, bool generateLightMapUv)
    {
        Cyan::Toolkit::ScopedTimer meshTimer(name, true);

        // get extension from mesh path
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        found = path.find_last_of('/');
        std::string baseDir = path.substr(0, found);

        Mesh* mesh = nullptr;
        if (extension == ".obj")
        {
            cyanInfo("Loading .obj file %s", path.c_str());
            mesh = loadObj(baseDir.c_str(), path.c_str(), generateLightMapUv);
        }
        else
            cyanError("Unsupported mesh file format %s", extension.c_str());

        // Store the xform for normalizing object space mesh coordinates
        mesh->m_name = name;
        mesh->m_bvh  = nullptr;
        mesh->m_shouldNormalize = normalize;
        mesh->onFinishLoading();
        return mesh;
    }

    void AssetManager::loadMeshes(Scene* scene, nlohmann::basic_json<std::map>& meshInfoList)
    {
        for (auto meshInfo : meshInfoList) 
        {
            std::string path, name;
            bool normalize, generateLightMapUv;
            meshInfo.at("path").get_to(path);
            meshInfo.at("name").get_to(name);
            meshInfo.at("normalize").get_to(normalize);
            meshInfo.at("generateLightMapUv").get_to(generateLightMapUv);

            auto mesh = loadMesh(path, name.c_str(), normalize, generateLightMapUv);
            Cyan::addMesh(mesh);
        }
    }

    void AssetManager::loadNodes(Scene* scene, nlohmann::basic_json<std::map>& nodeInfoList)
    {
        Cyan::Toolkit::ScopedTimer timer("loadNodes()", true);
        auto sceneManager = SceneManager::getSingletonPtr();
        for (auto nodeInfo : nodeInfoList)
        {
            u32 index = nodeInfo.at("index");
            std::string nodeName = nodeInfo.at("name");
            auto xformInfo = nodeInfo.at("xform");
            Transform transform = nodeInfo.at("xform").get<Transform>();
            if (nodeInfo.find("file") != nodeInfo.end())
            {
                std::string nodeFile = nodeInfo.at("file");
                SceneNode* node = nullptr; 
                if (nodeFile.find(".gltf") != std::string::npos) {
                    node = loadGltf(scene, nodeFile.c_str(), nodeName.c_str(), transform);
                }
                m_nodes.push_back(node);
                continue;
            }
            std::string meshName = nodeInfo.at("mesh");
            Cyan::Mesh* mesh = nullptr;
            mesh = Cyan::getMesh(meshName.c_str());
            SceneNode* node = sceneManager->createSceneNode(scene, nodeName.c_str(), transform, mesh); 
            m_nodes.push_back(node);
        }
        // second pass to setup the hierarchy
        for (auto nodeInfo : nodeInfoList)
        {
            u32 index = nodeInfo.at("index");
            std::vector<u32> childNodes = nodeInfo.at("child");
            for (auto child : childNodes)
            {
                SceneNode* childNode = m_nodes[child];
                m_nodes[index]->attach(childNode);
            }
        }
    }

    void AssetManager::loadEntities(::Scene* scene, nlohmann::basic_json<std::map>& entityInfoList)
    {
        Cyan::Toolkit::ScopedTimer timer("loadEntities()", true);
        for (auto entityInfo : entityInfoList)
        {
            std::string entityName;
            entityInfo.at("name").get_to(entityName);
            auto xformInfo = entityInfo.at("xform");
            Transform xform = entityInfo.at("xform").get<Transform>();
            Entity* entity = SceneManager::getSingletonPtr()->createEntity(scene, entityName.c_str(), xform, entityInfo.at("static"));
            auto sceneNodes = entityInfo.at("nodes");
            for (auto node : sceneNodes)
                entity->attachSceneNode(m_nodes[node]);
        }
    }

    void AssetManager::loadScene(Scene* scene, const char* file)
    {
        nlohmann::json sceneJson;
        std::ifstream sceneFile(file);
        try
        {
            sceneFile >> sceneJson;
        }
        catch (std::exception& e) 
        {
            std::cerr << e.what() << std::endl;
        }
        auto cameras = sceneJson["cameras"];
        auto meshInfoList = sceneJson["meshes"];
        auto textureInfoList = sceneJson["textures"];
        auto nodes = sceneJson["nodes"];
        auto entities = sceneJson["entities"];

        scene->activeCamera = 0u;
        u32 camIdx = 0u;
        for (auto& camera : cameras) 
        {
            scene->cameras[camIdx] = camera.get<Camera>();
            scene->cameras[camIdx].view = glm::mat4(1.f);
            scene->cameras[camIdx++].update();
        }

        loadTextures(textureInfoList);
        loadMeshes(scene, meshInfoList);
        loadNodes(scene, nodes);
        loadEntities(scene, entities);
    }

    Cyan::Texture* AssetManager::loadGltfTexture(const char* nodeName, tinygltf::Model& model, i32 index) {
        using Cyan::Texture;
        auto textureManager = Cyan::TextureManager::getSingletonPtr();
        Texture* texture = nullptr;
        if (index > -1) {
            auto gltfTexture = model.textures[index];
            auto image = model.images[gltfTexture.source];
            u32 sizeInBytes = image.image.size();
            Cyan::TextureSpec spec = { };
            spec.type = Texture::Type::TEX_2D;
            spec.width = image.width;
            spec.height = image.height;
            switch (image.component) {
                case 3: 
                {
                    if (image.bits == 8) 
                    {
                        spec.format = Cyan::Texture::ColorFormat::R8G8B8;
                        spec.dataType = Texture::DataType::UNSIGNED_BYTE;
                    } 
                    else if (image.bits == 16) 
                    {
                        spec.format = Cyan::Texture::ColorFormat::R16G16B16;
                        spec.dataType = Texture::DataType::Float;
                    } 
                    else if (image.bits == 32) 
                    {
                        spec.format = Cyan::Texture::ColorFormat::R32G32B32;
                        spec.dataType = Texture::DataType::Float;
                    } 
                    break;
                }
                case 4: {
                    if (image.bits == 8) 
                    {
                        spec.format = Cyan::Texture::ColorFormat::R8G8B8A8;
                        spec.dataType = Texture::DataType::UNSIGNED_BYTE;
                    } 
                    else if (image.bits == 16) 
                    {
                        spec.format = Cyan::Texture::ColorFormat::R16G16B16A16;
                        spec.dataType = Texture::DataType::Float;
                    } 
                    else if (image.bits == 32) 
                    {
                        spec.format = Cyan::Texture::ColorFormat::R32G32B32A32;
                        spec.dataType = Texture::DataType::Float;
                    } 
                    break;
                }
                default: 
                {
                    cyanError("Invalid number of channels when loading gltf image");
                    break;
                }
            }
            spec.min = Texture::Filter::LINEAR;
            spec.mag = Texture::Filter::LINEAR;
            spec.numMips = 11;
            spec.data = reinterpret_cast<void*>(image.image.data());
            u32 nameLen = (u32)strlen(image.uri.c_str());
            CYAN_ASSERT(nameLen < 128, "Texture filename too long!");
            char name[128];
            sprintf(name, "%s", image.uri.c_str());
            switch (spec.format) {
                case Texture::ColorFormat::R8G8B8:
                case Texture::ColorFormat::R8G8B8A8: 
                {
                    texture = textureManager->createTexture(name, spec);
                    break;
                }
                case Texture::ColorFormat::R16G16B16:
                case Texture::ColorFormat::R16G16B16A16: 
                case Texture::ColorFormat::R32G32B32:
                case Texture::ColorFormat::R32G32B32A32:
                {
                    texture = textureManager->createTextureHDR(name, spec);
                    break;
                }
                default:
                    break;
            }
        }
        return texture;
    }

    // TODO: Normalize mesh scale
    SceneNode* AssetManager::loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, 
                        SceneNode* parentSceneNode, tinygltf::Node& node, u32 numNodes) {
        auto textureManager = Cyan::TextureManager::getSingletonPtr();
        auto sceneManager = SceneManager::getSingletonPtr();
        bool hasMesh = (node.mesh > -1);
        bool hasSkin = (node.skin > -1);
        bool hasMatrix = !node.matrix.empty();
        bool hasTransformComponent = (!node.rotation.empty() || !node.translation.empty() ||
                                    !node.scale.empty());
        bool hasTransform = (hasMatrix || hasTransformComponent);
        const char* meshName = hasMesh ? model.meshes[node.mesh].name.c_str() : nullptr;
        Transform localTransform;
        if (hasMatrix) 
        {
            glm::mat4 matrix = {
                glm::vec4(node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3]),     // column 0
                glm::vec4(node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7]),     // column 1
                glm::vec4(node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11]),   // column 2
                glm::vec4(node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15])  // column 3
            };
            // convert matrix to local transform
            localTransform.fromMatrix(matrix);
        } 
        else if (hasTransformComponent) 
        {
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

        // create a SceneNode for this node
        char sceneNodeName[128];
        CYAN_ASSERT(node.name.size() < kEntityNameMaxLen, "Entity name too long !!")
        if (node.name.empty())
            sprintf_s(sceneNodeName, "Node%u", numNodes);
        else 
            sprintf_s(sceneNodeName, "%s", node.name.c_str());

        Cyan::Mesh* mesh = hasMesh ? Cyan::getMesh(meshName) : nullptr;
        SceneNode* sceneNode = sceneManager->createSceneNode(scene, sceneNodeName, localTransform, mesh);
        if (parentSceneNode)
            parentSceneNode->attach(sceneNode);
        // bind material
        Cyan::MeshInstance* meshInstance = sceneNode->m_meshInstance;
        if (meshInstance)
        {
            auto& gltfMesh = model.meshes[node.mesh];
            for (u32 sm = 0u; sm < gltfMesh.primitives.size(); ++sm) 
            {
                auto& primitive = gltfMesh.primitives[sm];
                Cyan::StandardPbrMaterial* matl = nullptr;
                if (primitive.material > -1) 
                {
                    auto& gltfMaterial = model.materials[primitive.material];
                    auto pbr = gltfMaterial.pbrMetallicRoughness;
                    PbrMaterialParam params = { };
                    auto getTexture = [&](i32 imageIndex) 
                    {
                        Cyan::Texture* texture = nullptr;
                        if (imageIndex > -1) 
                        {
                            auto& image = model.images[imageIndex];
                            texture = textureManager->getTexture(image.uri.c_str());
                        }
                        return texture;
                    };

                    params.baseColor = getTexture(pbr.baseColorTexture.index);
                    params.normal = getTexture(gltfMaterial.normalTexture.index);
                    params.metallicRoughness = getTexture(pbr.metallicRoughnessTexture.index);
                    params.occlusion = getTexture(gltfMaterial.occlusionTexture.index);
                    params.indirectDiffuseScale = 1.f;
                    params.indirectSpecularScale = 1.f;
                    matl = new StandardPbrMaterial(params);
                    meshInstance->m_matls[sm] = matl->m_materialInstance;
                }
                else
                {
                    matl = new StandardPbrMaterial;
                    meshInstance->m_matls[sm] = matl->m_materialInstance;
                }
                scene->addStandardPbrMaterial(matl);
            }
        }
        // recurse to load all the children
        for (auto& child : node.children) 
            loadGltfNode(scene, model, &node, sceneNode, model.nodes[child], ++numNodes);
        return sceneNode;
    }

    template <typename T>
    void loadVerticesAndIndices(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<typename T::Vertex>& vertices, std::vector<u32>& indices)
    {
         vertices.resize(model.accessors[primitive.attributes.begin()->second].count);

        // fill vertices
        for (auto itr = primitive.attributes.begin(); itr != primitive.attributes.end(); ++itr)
        {
            tinygltf::Accessor accessor = model.accessors[itr->second];
            tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
            tinygltf::Buffer buffer = model.buffers[bufferView.buffer];

            if (itr->first.compare("POSITION") == 0 && (T::Vertex::getFlags() && VertexAttribFlag::kPosition != 0))
            {
                // sanity checks (assume that position can only be vec3)
                CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3, "Position attributes in format other than Vec3 is not allowed")

                for (u32 v = 0; v < vertices.size(); ++v)
                {
                    f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                    vertices[v].pos = glm::vec3(src[v * 3], src[v * 3 + 1], src[v * 3 + 2]);
                }
            }
            else if (itr->first.compare("NORMAL") == 0 && (T::Vertex::getFlags() && VertexAttribFlag::kNormal != 0))
            {
                // sanity checks (assume that normal can only be vec3)
                CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3, "Normal attributes in format other than Vec3 is not allowed")

                for (u32 v = 0; v < vertices.size(); ++v)
                {
                    f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                    vertices[v].normal = glm::vec3(src[v * 3], src[v * 3 + 1], src[v * 3 + 2]);
                }
            }
            else if (itr->first.compare("TANGENT") == 0 && (T::Vertex::getFlags() && VertexAttribFlag::kTangents != 0))
            {
                // sanity checks (assume that tangent can only be in vec4)
                CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC4, "Position attributes in format other than Vec4 is not allowed")

                for (u32 v = 0; v < vertices.size(); ++v)
                {
                    f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                    vertices[v].tangent = glm::vec4(src[v * 4], src[v * 4 + 1], src[v * 4 + 2], src[v * 4 + 3]);
                }
            }
            else if (itr->first.find("TEXCOORD") == 0 && (T::Vertex::getFlags() && VertexAttribFlag::kTexcoord != 0))
            {
                // sanity checks (assume that texcoord can only be in vec2)
                CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC2, "TexCoord attributes in format other than Vec2 is not allowed")
                u32 texCoordIndex = 0;
                sscanf_s(itr->first.c_str(), "TEXCOORD_%u", &texCoordIndex);
                switch (texCoordIndex)
                {
                case 0:
                {
                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                        vertices[v].texCoord0 = glm::vec2(src[v * 2], src[v * 2 + 1]);
                    }
                } break;
                case 1:
                {

                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                        vertices[v].texCoord1 = glm::vec2(src[v * 2], src[v * 2 + 1]);
                    }
                } break;
                default:
                    break;
                }
            }
        }

        // fill indices 
        if (primitive.indices >= 0)
        {
            auto& accessor = model.accessors[primitive.indices];
            u32 numIndices = accessor.count;
            CYAN_ASSERT(numIndices % 3 == 0, "Invalid index count for triangle mesh")
            indices.resize(numIndices);
            tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
            tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
            u32 srcIndexSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
            switch (srcIndexSize)
            {
            case 1:
                for (u32 i = 0; i < numIndices; ++i)
                {
                    u8 index = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i];
                    indices.push_back(index);
                }
                break;
            case 2:
                for (u32 i = 0; i < numIndices; ++i)
                {
                    u8 byte0 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 0];
                    u8 byte1 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 1];
                    u32 index = (byte1 << 8) | byte0;
                    indices.push_back(index);
                }
                break;
            case 4:
                for (u32 i = 0; i < numIndices; ++i)
                {
                    u8 byte0 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 0];
                    u8 byte1 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 1];
                    u8 byte2 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 2];
                    u8 byte3 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 3];
                    u32 index = (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0;
                    indices.push_back(index);
                }
                break;
            default:
                break;
            }
        }
    }

    Cyan::Mesh* AssetManager::loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh) {

        Mesh* mesh = createMesh(gltfMesh.name.c_str());
        mesh->m_name = gltfMesh.name;
        mesh->m_bvh = nullptr;

        std::vector<BaseSubmesh*> submeshes;

        // primitives (submeshes)
        for (u32 i = 0u; i < (u32)gltfMesh.primitives.size(); ++i)
        {
            tinygltf::Primitive primitive = gltfMesh.primitives[i];

            switch (primitive.mode)
            {
            case TINYGLTF_MODE_TRIANGLES:
            {
                std::vector<Triangles::Vertex> vertices;
                std::vector<u32> indices;
                vertices.resize(model.accessors[primitive.attributes.begin()->second].count);

                // fill vertices
                for (auto itr = primitive.attributes.begin(); itr != primitive.attributes.end(); ++itr)
                {
                    tinygltf::Accessor accessor = model.accessors[itr->second];
                    tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                    tinygltf::Buffer buffer = model.buffers[bufferView.buffer];

                    if (itr->first.compare("POSITION") == 0)
                    {
                        // sanity checks (assume that position can only be vec3)
                        CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3, "Position attributes in format other than Vec3 is not allowed")

                        for (u32 v = 0; v < vertices.size(); ++v)
                        {
                            f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                            vertices[v].pos = glm::vec3(src[v * 3], src[v * 3 + 1], src[v * 3 + 2]);
                        }
                    }
                    else if (itr->first.compare("NORMAL") == 0)
                    {
                        // sanity checks (assume that normal can only be vec3)
                        CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3, "Normal attributes in format other than Vec3 is not allowed")

                        for (u32 v = 0; v < vertices.size(); ++v)
                        {
                            f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                            vertices[v].normal = glm::vec3(src[v * 3], src[v * 3 + 1], src[v * 3 + 2]);
                        }
                    }
                    else if (itr->first.compare("TANGENT") == 0)
                    {
                        // sanity checks (assume that tangent can only be in vec4)
                        CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC4, "Position attributes in format other than Vec4 is not allowed")

                        for (u32 v = 0; v < vertices.size(); ++v)
                        {
                            f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                            vertices[v].tangent = glm::vec4(src[v * 4], src[v * 4 + 1], src[v * 4 + 2], src[v * 4 + 3]);
                        }
                    }
                    else if (itr->first.find("TEXCOORD") == 0)
                    {
                        // sanity checks (assume that texcoord can only be in vec2)
                        CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC2, "TexCoord attributes in format other than Vec2 is not allowed")
                        u32 texCoordIndex = 0;
                        sscanf_s(itr->first.c_str(), "TEXCOORD_%u", &texCoordIndex);
                        switch (texCoordIndex)
                        {
                        case 0:
                        {
                            for (u32 v = 0; v < vertices.size(); ++v)
                            {
                                f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                                vertices[v].texCoord0 = glm::vec2(src[v * 2], src[v * 2 + 1]);
                            }
                        } break;
                        case 1:
                        {

                            for (u32 v = 0; v < vertices.size(); ++v)
                            {
                                f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                                vertices[v].texCoord1 = glm::vec2(src[v * 2], src[v * 2 + 1]);
                            }
                        } break;
                        default:
                            break;
                        }
                    }
                }

                // fill indices 
                if (primitive.indices >= 0)
                {
                    auto& accessor = model.accessors[primitive.indices];
                    u32 numIndices = accessor.count;
                    CYAN_ASSERT(numIndices % 3 == 0, "Invalid index count for triangle mesh")
                    indices.resize(numIndices);
                    tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                    tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
                    u32 srcIndexSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
                    switch (srcIndexSize)
                    {
                    case 1:
                        for (u32 i = 0; i < numIndices; ++i)
                        {
                            u8 index = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i];
                            indices.push_back(index);
                        }
                        break;
                    case 2:
                        for (u32 i = 0; i < numIndices; ++i)
                        {
                            u8 byte0 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 0];
                            u8 byte1 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 1];
                            u32 index = (byte1 << 8) | byte0;
                            indices.push_back(index);
                        }
                        break;
                    case 4:
                        for (u32 i = 0; i < numIndices; ++i)
                        {
                            u8 byte0 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 0];
                            u8 byte1 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 1];
                            u8 byte2 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 2];
                            u8 byte3 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 3];
                            u32 index = (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0;
                            indices.push_back(index);
                        }
                        break;
                    default:
                        break;
                    }
                    /*
                    // FIXME: type is hard-coded to u32 for now
                    u32* indexDataBuffer = new u32[numIndices];
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
                    subMesh->m_numIndices = numIndices;
                    // load cpu mesh data
                    u32 numFaces = numIndices / 3;
                    */
                }

                submeshes.push_back(createSubmesh<Triangles>(vertices, indices));
            } break;
            default:
                break;
            }

#if 0
            u32 numVertices = 0u, numIndices = 0u;
            // Convert data for each vertex attributes into raw buffer
            u32 strideInBytes = 0u;
            auto incrementVertexStride = [&](auto& attribute) {
                tinygltf::Accessor accessor = model.accessors[attribute.second];
                if (numVertices > 0u)
                {
                    CYAN_ASSERT(numVertices == accessor.count, "Mismatch vertex count among vertex attributes")
                }
                else
                    numVertices = accessor.count;
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
                const auto& attrib = primitive.attributes.find(entry.name);
                tinygltf::Accessor accessor = model.accessors[attrib->second];
                tinygltf::BufferView bufferView = model.bufferViews[accessor.bufferView];
                tinygltf::Buffer buffer = model.buffers[bufferView.buffer];
                u8* srcStart = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                u8* dstStart = reinterpret_cast<u8*>(vertexDataBuffer) + offset;
                u32 sizeToCopy = tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
                u32 numComponents = tinygltf::GetNumComponentsInType(accessor.type);
                for (u32 v = 0; v < numVertices; ++v)
                {
                    void* srcDataAddress = reinterpret_cast<void*>(srcStart + v * bufferView.byteStride);
                    void* dstDataAddress = reinterpret_cast<void*>(dstStart + (u64)(v * strideInBytes)); 
                    memcpy(dstDataAddress, srcDataAddress, sizeToCopy);
                    // TODO: Do this in a not so hacky way 
                    // flip the y-component of texcoord
                    if (entry.name.find("TEXCOORD") == 0) {
                        float* data = reinterpret_cast<float*>(dstDataAddress);
                        data[1] = 1.f - data[1];
                    }
                    totalBytes += sizeToCopy;
                }
                // FIXME: type is hard-coded to float for now
                vertexAttribs.push_back({
                    VertexAttrib::DataType::Float,
                    static_cast<u32>(tinygltf::GetNumComponentsInType(accessor.type)),
                    strideInBytes,
                    offset
                });
                offset += sizeToCopy;
            }
            CYAN_ASSERT(totalBytes == strideInBytes * numVertices, "mismatched buffer size")
            VertexBuffer* vb = Cyan::createVertexBuffer(reinterpret_cast<void*>(vertexDataBuffer), strideInBytes * numVertices, strideInBytes, numVertices);
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
                // FIXME: type is hard-coded to u32 for now
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
                subMesh->m_numIndices = numIndices;
                // load cpu mesh data
                u32 numFaces = numIndices / 3;
                CYAN_ASSERT(numIndices % 3 == 0, "Given gltf mesh has invalid index buffer!");
#if 0
                for (u32 f = 0; f < numFaces; ++f)
                {
                    for (u32 v = 0; v < 3; ++v)
                    {
                        u32 offset = strideInBytes * indexDataBuffer[f * 3 + v];
                        f32* srcDataAddress = (f32*)((u8*)vertexDataBuffer + offset);
                        subMesh->m_triangles.m_positionArray.emplace_back(srcDataAddress[0], srcDataAddress[1], srcDataAddress[2]);
                        subMesh->m_triangles.m_normalArray.emplace_back(srcDataAddress[3], srcDataAddress[4], srcDataAddress[5]);
                        subMesh->m_triangles.m_tangentArray.emplace_back(srcDataAddress[6], srcDataAddress[7], srcDataAddress[8]);
                        subMesh->m_triangles.m_texCoordArray.emplace_back(srcDataAddress[10], srcDataAddress[11], 0.f);
                    }
                }
                subMesh->m_triangles.m_numVerts = numIndices;
#endif
                delete[] vertexDataBuffer;
                delete[] indexDataBuffer;
            }
            mesh->m_subMeshes.push_back(subMesh);
#endif
        } // primitive (submesh)
        mesh->m_normalization = glm::mat4(1.0);
        mesh->m_shouldNormalize = false;
        mesh->onFinishLoading();
        return mesh;
    }

    void AssetManager::loadGltfTextures(const char* nodeName, tinygltf::Model& model) {
        using Cyan::Texture;
        for (u32 t = 0u; t < model.textures.size(); ++t) {
            loadGltfTexture(nodeName, model, t);
        }
    }

    SceneNode* AssetManager::loadGltf(Scene* scene, const char* filename, const char* name, Transform transform)
    {
        using Cyan::Mesh;
        tinygltf::Model model;
        std::string warn, err;
        auto sceneManager = SceneManager::getSingletonPtr();
        if (!m_loader.LoadASCIIFromFile(&model, &err, &warn, std::string(filename)))
        {
            std::cout << warn << std::endl;
            std::cout << err << std::endl;
        }
        tinygltf::Scene& gltfScene = model.scenes[model.defaultScene];
        // load textures
        loadGltfTextures(name, model);
        // load meshes
        for (auto& gltfMesh : model.meshes)
        {
            Mesh* mesh = loadGltfMesh(model, gltfMesh);
            Cyan::addMesh(mesh);
        }
        // todo: Handle multiple root nodes
        // assuming that there is only one root node for defaultScene
        tinygltf::Node rootNode = model.nodes[gltfScene.nodes[0]];
        Cyan::Mesh* rootNodeMesh = nullptr;
        if (rootNode.mesh > 0)
        {
            tinygltf::Mesh gltfMesh = model.meshes[rootNode.mesh];
            rootNodeMesh = Cyan::getMesh(gltfMesh.name.c_str());
        }
        SceneNode* parentNode = sceneManager->createSceneNode(scene, name, transform, nullptr);
        loadGltfNode(scene, model, nullptr, parentNode, rootNode, 0);
        return parentNode;
    }

    template <typename T>
    Material<T>* AssetManager::getAsset(const char* matlName)
    {
    }
}