#include <fstream>

#define GLM_ENABLE_EXPERIMENTAL
#include "gtx/hash.hpp"
#include "tiny_obj_loader.h"
#include "xatlas.h"

#include "AssetManager.h"
#include "Texture.h"
#include "CyanAPI.h"

namespace std {
    template<> 
    struct hash<Cyan::Triangles::Vertex> 
    {
        size_t operator()(const Cyan::Triangles::Vertex& vertex) const 
        {
            size_t a = hash<glm::vec3>()(vertex.pos); 
            size_t b = hash<glm::vec3>()(vertex.normal); 
            size_t c = hash<glm::vec2>()(vertex.texCoord0);
            size_t d = hash<glm::vec2>()(vertex.texCoord1); 
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
    bool operator==(const Triangles::Vertex& lhs, const Triangles::Vertex& rhs)
    {
        // todo: maybe use memcmp() here instead ..?
        // bit equivalence
        return (lhs.pos == rhs.pos)
            && (lhs.normal == rhs.normal)
            && (lhs.tangent == rhs.tangent)
            && (lhs.texCoord0 == rhs.texCoord0);
    }

    AssetManager* AssetManager::singleton = nullptr;
    AssetManager::AssetManager()
    {
        if (!singleton)
        {
            singleton = this;
        }
    }

    void AssetManager::importTextures(nlohmann::basic_json<std::map>& textureInfoList)
    {
        using Cyan::Texture2DRenderable;
        for (auto textureInfo : textureInfoList) 
        {
            std::string filename = textureInfo.at("path").get<std::string>();
            std::string name     = textureInfo.at("name").get<std::string>();
            std::string dynamicRange = textureInfo.at("dynamic_range").get<std::string>();
            u32 numMips = textureInfo.at("numMips").get<u32>();
            
            ITextureRenderable::Spec spec = { };
            spec.numMips = numMips;
            Texture2DRenderable* texture = nullptr;
            texture = importTexture2D(
                name.c_str(), 
                filename.c_str(), 
                spec,
                ITextureRenderable::Parameter { 
                    ITextureRenderable::Parameter::Filtering::LINEAR,
                    ITextureRenderable::Parameter::Filtering::LINEAR,
                    ITextureRenderable::Parameter::WrapMode::WRAP,
                    ITextureRenderable::Parameter::WrapMode::WRAP,
                    ITextureRenderable::Parameter::WrapMode::WRAP
                });
            m_textureMap.insert({ name.c_str(), texture });
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

#if 0
    void addSubmeshToLightmap(xatlas::Atlas* atlas, const std::vector<Triangles::Vertex>& vertices, const std::vector<u32>& indices)
    {
        xatlas::SetPrint(Print, true);
        xatlas::MeshDecl meshDecl;
        meshDecl.vertexCount = vertices.size();
        meshDecl.vertexPositionData = &vertices[0].pos.x;
        meshDecl.vertexPositionStride = sizeof(Triangles::Vertex);
        meshDecl.vertexNormalData = &vertices[0].normal.x;
        meshDecl.vertexNormalStride = sizeof(Triangles::Vertex);
        meshDecl.vertexUvData = &vertices[0].texCoord0.x;
        meshDecl.vertexUvStride = sizeof(Triangles::Vertex);
        meshDecl.indexCount = (u32)indices.size();
        meshDecl.indexData = indices.data();
        meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);
        if (error != xatlas::AddMeshError::Success) 
            cyanError("Error adding mesh");
    }
#endif

    void calculateTangent(std::vector<Triangles::Vertex>& vertices, u32 face[3])
    {
        auto& v0 = vertices[face[0]];
        auto& v1 = vertices[face[1]];
        auto& v2 = vertices[face[2]];
        glm::vec3 v0v1 = v1.pos - v0.pos;
        glm::vec3 v0v2 = v2.pos - v0.pos;
        glm::vec2 deltaUv0 = v1.texCoord0 - v0.texCoord0;
        glm::vec2 deltaUv1 = v2.texCoord0 - v0.texCoord0;
        f32 tx = (deltaUv1.y * v0v1.x - deltaUv0.y * v0v2.x) / (deltaUv0.x * deltaUv1.y - deltaUv1.x * deltaUv0.y);
        f32 ty = (deltaUv1.y * v0v1.y - deltaUv0.y * v0v2.y) / (deltaUv0.x * deltaUv1.y - deltaUv1.x * deltaUv0.y);
        f32 tz = (deltaUv1.y * v0v1.z - deltaUv0.y * v0v2.z) / (deltaUv0.x * deltaUv1.y - deltaUv1.x * deltaUv0.y);
        glm::vec3 tangent(tx, ty, tz);
        tangent = glm::normalize(tangent);
        v0.tangent = glm::vec4(tangent, 1.f);
        v1.tangent = glm::vec4(tangent, 1.f);
        v2.tangent = glm::vec4(tangent, 1.f);
    }

    // treat all the meshes inside one obj file as submeshes
    std::vector<ISubmesh*> AssetManager::loadObj(const char* baseDir, const char* filename, bool bGenerateLightMapUv)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;

        auto addSubmeshToLightmap = [](xatlas::Atlas* atlas, const std::vector<Triangles::Vertex>& vertices, const std::vector<u32>& indices) {
            xatlas::SetPrint(Print, true);
            xatlas::MeshDecl meshDecl;
            meshDecl.vertexCount = vertices.size();
            meshDecl.vertexPositionData = &vertices[0].pos.x;
            meshDecl.vertexPositionStride = sizeof(Triangles::Vertex);
            meshDecl.vertexNormalData = &vertices[0].normal.x;
            meshDecl.vertexNormalStride = sizeof(Triangles::Vertex);
            meshDecl.vertexUvData = &vertices[0].texCoord0.x;
            meshDecl.vertexUvStride = sizeof(Triangles::Vertex);
            meshDecl.indexCount = (u32)indices.size();
            meshDecl.indexData = indices.data();
            meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

            xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl);
            if (error != xatlas::AddMeshError::Success) 
                cyanError("Error adding mesh");
        };

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, baseDir);
        if (!ret)
        {
            cyanError("Failed loading obj file %s ", filename);
            cyanError("Warnings: %s               ", warn.c_str());
            cyanError("Errors:   %s               ", err.c_str());
        }
        auto atlas = xatlas::Create();
        std::vector<ISubmesh*> submeshes;
        for (u32 s = 0; s < shapes.size(); ++s)
        {
            // load triangle mesh
            if (shapes[s].mesh.indices.size() > 0)
            {
                std::vector<Triangles::Vertex> vertices;
                std::vector<u32> indices(shapes[s].mesh.indices.size());

                std::unordered_map<Triangles::Vertex, u32> uniqueVerticesMap;
                u32 numUniqueVertices = 0;

                // assume that one submesh can only have one material
                /*
                if (shapes[s].mesh.material_ids.size() > 0)
                {
                    subMesh->m_materialIdx = shapes[s].mesh.material_ids[0];
                }
                */
                // load triangles
                for (u32 f = 0; f < shapes[s].mesh.indices.size() / 3; ++f)
                {
                    Triangles::Vertex vertex = { };
                    u32 face[3] = { };

                    for (u32 v = 0; v < 3; ++v)
                    {
                        tinyobj::index_t index = shapes[s].mesh.indices[f * 3 + v];
                        // position
                        f32 vx = attrib.vertices[index.vertex_index * 3 + 0];
                        f32 vy = attrib.vertices[index.vertex_index * 3 + 1];
                        f32 vz = attrib.vertices[index.vertex_index * 3 + 2];
                        vertex.pos = glm::vec3(vx, vy, vz);
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
                            vertex.texCoord0 = glm::vec2(tx, ty);
                        }
                        else
                            vertex.texCoord0 = glm::vec2(0.f);

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
                    }

                    // compute face tangent
                    calculateTangent(vertices, face);
                }

                submeshes.push_back(createSubmesh<Triangles>(vertices, indices));

                if (bGenerateLightMapUv)
                {
                    addSubmeshToLightmap(atlas, vertices, indices);
                }
            } 
            // load lines
            else if (shapes[s].lines.indices.size() > 0)
            {
                std::vector<Lines::Vertex> vertices;
                std::vector<u32> indices;
                vertices.resize(shapes[s].lines.num_line_vertices.size());
                indices.resize(shapes[s].lines.indices.size());

                for (u32 l = 0; l < shapes[s].lines.indices.size() / 2; ++l)
                {
                    Lines::Vertex vertex = { };
                    for (u32 v = 0; v < 2; ++v)
                    {
                        tinyobj::index_t index = shapes[s].lines.indices[l * 2 + v];
                        f32 vx = attrib.vertices[index.vertex_index * 3 + 0];
                        f32 vy = attrib.vertices[index.vertex_index * 3 + 1];
                        f32 vz = attrib.vertices[index.vertex_index * 3 + 2];
                        vertex.pos = glm::vec3(vx, vy, vz);
                        vertices[index.vertex_index] = vertex;
                        indices[l * 2 + v] = index.vertex_index;
                    }
                }
            }
        }

        // generating lightmap uv if requested
        if (bGenerateLightMapUv)
        {
            // atlas now holds results of packing
            xatlas::PackOptions packOptions = { };
            packOptions.bruteForce = true;
            packOptions.padding = 5.f;
            packOptions.resolution = 1024;

            xatlas::Generate(atlas, xatlas::ChartOptions{}, packOptions);
            CYAN_ASSERT(atlas->meshCount == submeshes.size(), "# Submeshes and # of meshes in atlas doesn't match!");

            for (u32 i = 0; i < submeshes.size(); ++i)
            {
                // it's safe to do this cast here as we are sure that all the submeshes are of type Triangles
                auto sm = static_cast<Mesh::Submesh<Triangles>*>(submeshes[i]);
                auto origVertices = sm->getVertices();
                std::vector<Triangles::Vertex> packedVertices(atlas->meshes[i].vertexCount);
                std::vector<u32>       packedIndices(atlas->meshes[i].indexCount);
                for (u32 v = 0; v < atlas->meshes[i].vertexCount; ++v)
                {
                    xatlas::Vertex atlasVertex = atlas->meshes[i].vertexArray[v];
                    packedVertices[v] = origVertices[atlasVertex.xref];
                    packedVertices[v].texCoord1.x = atlasVertex.uv[0] / atlas->width;
                    packedVertices[v].texCoord1.y = atlasVertex.uv[1] / atlas->height;
                }
                for (u32 ii = 0; ii < atlas->meshes[i].indexCount; ++ii)
                {
                    packedIndices[ii] = atlas->meshes[i].indexArray[ii];
                }

                sm->setGeometryData(packedVertices, packedIndices);
            }
        }
        xatlas::Destroy(atlas);
        return std::move(submeshes);
#if 0
        for (u32 i = 0; i < materials.size(); ++i)
        {
            mesh->m_objMaterials.emplace_back();
            auto& objMatl = mesh->m_objMaterials.back();
            objMatl.diffuse = glm::vec3{ materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2] };
            objMatl.specular = glm::vec3{ materials[i].specular[0], materials[i].specular[2], materials[i].specular[3] };
            objMatl.kMetalness = materials[i].metallic;
            objMatl.kRoughness = materials[i].roughness;
        }
#endif
    }

    Mesh* AssetManager::loadMesh(std::string& path, const char* name, bool normalize, bool generateLightMapUv)
    {
        Cyan::Toolkit::ScopedTimer meshTimer(name, true);

        // get extension from mesh path
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        found = path.find_last_of('/');
        std::string baseDir = path.substr(0, found);

        Mesh* parent = nullptr;
        if (extension == ".obj")
        {
            cyanInfo("Loading .obj file %s", path.c_str());
            std::vector<ISubmesh*> submeshes = std::move(loadObj(baseDir.c_str(), path.c_str(), generateLightMapUv));
            parent = createMesh(name, submeshes);
        }
        else
            cyanError("Unsupported mesh file format %s", extension.c_str());

        // todo: (handle converting mesh data for raster pipeline to ray tracing pipeline)
        // mesh->m_bvh  = nullptr;
        return parent;
    }

    void AssetManager::importMeshes(Scene* scene, nlohmann::basic_json<std::map>& meshInfoList)
    {
        for (auto meshInfo : meshInfoList) 
        {
            std::string path, name;
            bool normalize, generateLightMapUv;
            meshInfo.at("path").get_to(path);
            meshInfo.at("name").get_to(name);
            meshInfo.at("normalize").get_to(normalize);
            meshInfo.at("generateLightMapUv").get_to(generateLightMapUv);

            loadMesh(path, name.c_str(), normalize, generateLightMapUv);
        }
    }

    void AssetManager::importSceneNodes(Scene* scene, nlohmann::basic_json<std::map>& nodeInfoList)
    {
        Cyan::Toolkit::ScopedTimer timer("loadNodes()", true);
        auto sceneManager = SceneManager::get();
        for (auto nodeInfo : nodeInfoList)
        {
            u32 index = nodeInfo.at("index");
            std::string nodeName = nodeInfo.at("name");
            auto xformInfo = nodeInfo.at("xform");
            Transform transform = nodeInfo.at("xform").get<Transform>();
            if (nodeInfo.find("file") != nodeInfo.end())
            {
                std::string nodeFile = nodeInfo.at("file");
                SceneComponent* node = nullptr; 
                if (nodeFile.find(".gltf") != std::string::npos) {
                    node = loadGltf(scene, nodeFile.c_str(), nodeName.c_str(), transform);
                }
                m_nodes.push_back(node);
                continue;
            }
            std::string meshName = nodeInfo.at("mesh");
            Mesh* parent = getAsset<Mesh>(meshName.c_str());
            SceneComponent* node = scene->createMeshComponent(transform, parent); 
            m_nodes.push_back(node);
        }
        // second pass to setup the hierarchy
        for (auto nodeInfo : nodeInfoList)
        {
            u32 index = nodeInfo.at("index");
            std::vector<u32> childNodes = nodeInfo.at("child");
            for (auto child : childNodes)
            {
                SceneComponent* childNode = m_nodes[child];
                m_nodes[index]->attachChild(childNode);
            }
        }
    }

    void AssetManager::importEntities(Scene* scene, nlohmann::basic_json<std::map>& entityInfoList)
    {
        Cyan::Toolkit::ScopedTimer timer("loadEntities()", true);
        for (auto entityInfo : entityInfoList)
        {
            std::string entityName;
            entityInfo.at("name").get_to(entityName);
            auto xformInfo = entityInfo.at("xform");
            Transform xform = entityInfo.at("xform").get<Transform>();
            bool isStatic = entityInfo.at("static");
            u32 properties = (EntityFlag_kVisible | EntityFlag_kCastShadow);
            if (isStatic)
            {
                properties |= EntityFlag_kStatic;
            }
            else
            {
                properties |= EntityFlag_kDynamic;
            }
            Entity* entity = scene->createEntity(entityName.c_str(), xform, nullptr, properties);
            auto sceneNodes = entityInfo.at("nodes");
            for (auto node : sceneNodes)
            {
                entity->attachSceneComponent(m_nodes[node]);
            }
        }
    }

    void AssetManager::importScene(Scene* scene, const char* file)
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

        for (auto& camera : cameras) 
        {
            scene->camera = camera.get<Camera>();
            scene->camera.view = glm::mat4(1.f);
            scene->camera.update();
        }

        importTextures(textureInfoList);
        importMeshes(scene, meshInfoList);
        importSceneNodes(scene, nodes);
        importEntities(scene, entities);
    }

    Cyan::Texture2DRenderable* AssetManager::loadGltfTexture(const char* nodeName, tinygltf::Model& model, i32 index) {
        using Cyan::Texture2DRenderable;
        Texture2DRenderable* texture = nullptr;
        if (index > -1) {
            auto gltfTexture = model.textures[index];
            auto image = model.images[gltfTexture.source];
            u32 sizeInBytes = image.image.size();
            Cyan::ITextureRenderable::Spec spec = { };
            spec.width = image.width;
            spec.height = image.height;
            switch (image.component) {
                case 3: 
                {
                    if (image.bits == 8) 
                    {
                        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R8G8B8;
                    } 
                    else if (image.bits == 16) 
                    {
                        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16;
                    } 
                    else if (image.bits == 32) 
                    {
                        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32G32B32;
                    } 
                    break;
                }
                case 4: {
                    if (image.bits == 8) 
                    {
                        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R8G8B8A8;
                    } 
                    else if (image.bits == 16) 
                    {
                        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R16G16B16A16;
                    } 
                    else if (image.bits == 32) 
                    {
                        spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::R32G32B32A32;
                    } 
                    break;
                }
                default: 
                {
                    cyanError("Invalid number of channels when loading gltf image");
                    break;
                }
            }
            spec.numMips = 11;
            spec.pixelData = reinterpret_cast<u8*>(image.image.data());
            u32 nameLen = (u32)strlen(image.uri.c_str());
            CYAN_ASSERT(nameLen < 128, "Texture filename too long!");
            char name[128];
            sprintf(name, "%s", image.uri.c_str());
            texture = createTexture2D(name, spec);
        }
        return texture;
    }

    // TODO: Normalize mesh scale
    SceneComponent* AssetManager::loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, 
                        SceneComponent* parentSceneComponent, tinygltf::Node& node, u32 numNodes) {
        auto sceneManager = SceneManager::get();
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

        // create a SceneComponent for this node
        char sceneNodeName[128];
        CYAN_ASSERT(node.name.size() < kEntityNameMaxLen, "Entity name too long !!")
        if (node.name.empty())
            sprintf_s(sceneNodeName, "Node%u", numNodes);
        else 
            sprintf_s(sceneNodeName, "%s", node.name.c_str());
        
        SceneComponent* sceneComponent = nullptr;
        if (hasMesh)
        {
            Mesh* mesh = getAsset<Mesh>(meshName);
            sceneComponent = scene->createMeshComponent(localTransform, mesh);
        }
        else
        {
            sceneComponent = scene->createSceneComponent(sceneNodeName, localTransform);
        }
        if (parentSceneComponent)
            parentSceneComponent->attachChild(sceneComponent);
        // bind material
        if (auto meshInstance = sceneComponent->getAttachedMesh())
        {
            auto& gltfMesh = model.meshes[node.mesh];
            for (u32 sm = 0u; sm < gltfMesh.primitives.size(); ++sm) 
            {
                PBRMatl* matl = nullptr;

                auto& primitive = gltfMesh.primitives[sm];
                if (primitive.material > -1) 
                {
                    auto getTexture = [&](i32 imageIndex) 
                    {
                        Cyan::Texture2DRenderable* texture = nullptr;
                        if (imageIndex > -1) 
                        {
                            auto& image = model.images[imageIndex];
                            texture = getAsset<Texture2DRenderable>(image.uri.c_str());
                        }
                        return texture;
                    };

                    auto& gltfMaterial = model.materials[primitive.material];
                    auto pbr = gltfMaterial.pbrMetallicRoughness;

                    matl = createMaterial<PBRMatl>(gltfMaterial.name.c_str());
                    matl->parameter.albedo = getTexture(pbr.baseColorTexture.index);
                    matl->parameter.normal = getTexture(gltfMaterial.normalTexture.index);
                    matl->parameter.metallicRoughness = getTexture(pbr.metallicRoughnessTexture.index);
                    matl->parameter.occlusion = getTexture(gltfMaterial.occlusionTexture.index);
                }

                meshInstance->materials[sm] = matl;
            }
        }
        // recurse to load all the children
        for (auto& child : node.children) 
            loadGltfNode(scene, model, &node, sceneComponent, model.nodes[child], ++numNodes);
        return sceneComponent;
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

            if (itr->first.compare("POSITION") == 0 && (T::Vertex::getFlags() && VertexAttribFlag_kPosition != 0))
            {
                // sanity checks (assume that position can only be vec3)
                CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3, "Position attributes in format other than Vec3 is not allowed")

                for (u32 v = 0; v < vertices.size(); ++v)
                {
                    f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                    vertices[v].pos = glm::vec3(src[v * 3], src[v * 3 + 1], src[v * 3 + 2]);
                }
            }
            else if (itr->first.compare("NORMAL") == 0 && (T::Vertex::getFlags() && VertexAttribFlag_kNormal != 0))
            {
                // sanity checks (assume that normal can only be vec3)
                CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC3, "Normal attributes in format other than Vec3 is not allowed")

                for (u32 v = 0; v < vertices.size(); ++v)
                {
                    f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                    vertices[v].normal = glm::vec3(src[v * 3], src[v * 3 + 1], src[v * 3 + 2]);
                }
            }
            else if (itr->first.compare("TANGENT") == 0 && (T::Vertex::getFlags() && VertexAttribFlag_kTangent != 0))
            {
                // sanity checks (assume that tangent can only be in vec4)
                CYAN_ASSERT(accessor.type == TINYGLTF_TYPE_VEC4, "Position attributes in format other than Vec4 is not allowed")

                for (u32 v = 0; v < vertices.size(); ++v)
                {
                    f32* src = reinterpret_cast<f32*>(buffer.data.data() + (accessor.byteOffset + bufferView.byteOffset));
                    vertices[v].tangent = glm::vec4(src[v * 4], src[v * 4 + 1], src[v * 4 + 2], src[v * 4 + 3]);
                }
            }
            else if (itr->first.find("TEXCOORD") == 0 && (T::Vertex::getFlags() && VertexAttribFlag_kTexCoord0 != 0))
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

    Cyan::Mesh* AssetManager::loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh) 
    {

        std::vector<ISubmesh*> submeshes;

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
                }

                submeshes.push_back(createSubmesh<Triangles>(vertices, indices));
            } break;
            case TINYGLTF_MODE_LINE:
            case TINYGLTF_MODE_POINTS:
            case TINYGLTF_MODE_LINE_STRIP:
            case TINYGLTF_MODE_TRIANGLE_STRIP:
            default:
                break;
            }
        } // primitive (submesh)

        Mesh* parent = createMesh(gltfMesh.name.c_str(), submeshes);
        // todo: (handle converting mesh data for raster pipeline to ray tracing pipeline)
        // mesh->m_bvh = nullptr;
        return parent;
    }

    void AssetManager::loadGltfTextures(const char* nodeName, tinygltf::Model& model) {
        for (u32 t = 0u; t < model.textures.size(); ++t) {
            loadGltfTexture(nodeName, model, t);
        }
    }

    SceneComponent* AssetManager::loadGltf(Scene* scene, const char* filename, const char* name, Transform transform)
    {
        using Cyan::Mesh;
        tinygltf::Model model;
        std::string warn, err;
        auto sceneManager = SceneManager::get();
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
            Mesh* parent = loadGltfMesh(model, gltfMesh);
        }
        // todo: Handle multiple root nodes
        // assuming that there is only one root node for defaultScene
        tinygltf::Node rootNode = model.nodes[gltfScene.nodes[0]];
        Cyan::Mesh* rootNodeMesh = nullptr;
        if (rootNode.mesh > 0)
        {
            tinygltf::Mesh gltfMesh = model.meshes[rootNode.mesh];
            rootNodeMesh = getAsset<Mesh>(gltfMesh.name.c_str());
        }
        SceneComponent* parentNode = scene->createSceneComponent(name, transform);
        loadGltfNode(scene, model, nullptr, parentNode, rootNode, 0);
        return parentNode;
    }
}