#include <fstream>

#define GLM_ENABLE_EXPERIMENTAL
#include "gtx/hash.hpp"
#include "tiny_obj_loader.h"

#include "Asset.h"
#include "Texture.h"
#include "CyanAPI.h"

// treat this as a string and hash it ...?
struct ObjVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 texCoord;
};

bool operator==(const ObjVertex& lhs, const ObjVertex& rhs)
{
    return (lhs.position == rhs.position) 
        && (lhs.normal == rhs.normal) 
        && (lhs.texCoord == rhs.texCoord);
}

namespace std {
    template<> 
    struct hash<ObjVertex> 
    {
        size_t operator()(ObjVertex const& vertex) const 
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
            spec.m_type = Texture::Type::TEX_2D;
            spec.m_numMips = numMips;
            spec.m_min = Texture::Filter::LINEAR;
            spec.m_mag = Texture::Filter::LINEAR;
            spec.m_s = Texture::Wrap::NONE;
            spec.m_t = Texture::Wrap::NONE;
            spec.m_r = Texture::Wrap::NONE;
            if (dynamicRange == "ldr")
            {
                textureManager->createTexture(name.c_str(), filename.c_str(), spec);
            }
            else if (dynamicRange == "hdr")
            {
                textureManager->createTextureHDR(name.c_str(), filename.c_str(), spec);
            }
        }
    }

    void computeTangent(std::vector<ObjVertex>& vertices, u32 face[3])
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

    // treat all the meshes inside one obj file as submeshes
    Mesh* AssetManager::loadObj(const char* baseDir, const char* filename)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;
        bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, baseDir);
        if (!ret)
        {
            printf("Failed loading obj file %s \n", filename);
            printf("Warnings: %s               \n", warn.c_str());
            printf("Errors:   %s               \n", err.c_str());
        }

        Mesh* mesh = new Mesh;

        materials.push_back(tinyobj::material_t());

        for (u32 s = 0; s < shapes.size(); ++s)
        {
            printf("shape[%d].name = %s\n", s, shapes[s].name.c_str());
            std::vector<ObjVertex> vertices;
            std::vector<u32>       indices(shapes[s].mesh.indices.size());
            std::unordered_map<ObjVertex, u32> vertexMap;
            auto objMesh = shapes[s].mesh;
            u32 numUniqueVertices = 0;
            u32 strideInBytes = sizeof(ObjVertex);
            Mesh::SubMesh* subMesh = new Mesh::SubMesh;

            // indices
            for (u32 f = 0; f < shapes[s].mesh.indices.size() / 3; ++f)
            {
                ObjVertex vertex = { };
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
                    auto iter = vertexMap.find(vertex);
                    if (iter == vertexMap.end())
                    {
                        vertexMap[vertex] = numUniqueVertices;
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
            auto vb = Cyan::createVertexBuffer(vertices.data(), sizeof(vertices[0]) * numUniqueVertices, strideInBytes, numUniqueVertices);
            u32 offset = 0;
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 3, strideInBytes, offset});
            offset += 3 * sizeof(f32);
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 3, strideInBytes, offset});
            offset += 3 * sizeof(f32);
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 4, strideInBytes, offset});
            offset += 4 * sizeof(f32);
            vb->addVertexAttrib({ VertexAttrib::DataType::Float, 2, strideInBytes, offset});
            offset += 2 * sizeof(f32);
            subMesh->m_vertexArray = Cyan::createVertexArray(vb);
            subMesh->m_vertexArray->init();
            subMesh->m_numVerts = numUniqueVertices;
            subMesh->m_triangles.m_numVerts = indices.size();
            // create index buffer
            glCreateBuffers(1, &subMesh->m_vertexArray->m_ibo);
            glBindVertexArray(subMesh->m_vertexArray->m_vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh->m_vertexArray->m_ibo);
            glNamedBufferData(subMesh->m_vertexArray->m_ibo, indices.size() * sizeof(u32), indices.data(), GL_STATIC_DRAW);
            glBindVertexArray(0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            subMesh->m_vertexArray->m_numIndices = indices.size();
            mesh->m_subMeshes.push_back(subMesh);
        }
        return mesh;
    }

    Mesh* AssetManager::loadMesh(std::string& path, const char* name, bool normalize)
    {
        Cyan::Toolkit::ScopedTimer meshTimer(name, true);

        // get extension from mesh path
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        std::string baseDir   = path.substr(0, found);
        printf("The mesh file extension is %s", extension.c_str());
        Mesh* mesh = nullptr;
        if (extension == ".obj")
             mesh = loadObj(baseDir.c_str(), path.c_str());
        else if (extension == ".gltf") { }
        else
            printf("Unsupported mesh file format %s", extension.c_str());
        // Store the xform for normalizing object space mesh coordinates
        mesh->m_name = name;
        mesh->m_bvh  = nullptr;
        mesh->m_shouldNormalize = normalize;
        mesh->onFinishLoading();
        return mesh;
    }

    void AssetManager::loadMeshes(Scene* scene, nlohmann::basic_json<std::map>& meshInfoList)
    {
        Thekla::Atlas_Options options = { };
        Thekla::atlas_set_default_options(&options);

        for (auto meshInfo : meshInfoList) 
        {
            std::string path, name;
            bool normalize;
            meshInfo.at("path").get_to(path);
            meshInfo.at("name").get_to(name);
            meshInfo.at("normalize").get_to(normalize);
            
            auto mesh = loadMesh(path, name.c_str(), normalize);
            Cyan::addMesh(mesh);
        }
    }

    void AssetManager::loadNodes(Scene* scene, nlohmann::basic_json<std::map>& nodeInfoList)
    {
        Cyan::Toolkit::ScopedTimer timer("loadNodes()", true);
        for (auto nodeInfo : nodeInfoList)
        {
            u32 index = nodeInfo.at("index");
            std::string nodeName = nodeInfo.at("name");
            auto xformInfo = nodeInfo.at("xform");
            Transform transform = nodeInfo.at("xform").get<Transform>();
            std::string meshName = nodeInfo.at("mesh");
            Cyan::Mesh* mesh = nullptr;
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
            mesh = Cyan::getMesh(meshName.c_str());
            SceneNode* node = Cyan::createSceneNode(nodeName.c_str(), transform, mesh); 
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

    void AssetManager::loadEntities(Scene* scene, nlohmann::basic_json<std::map>& entityInfoList)
    {
        Cyan::Toolkit::ScopedTimer timer("loadEntities()", true);
        for (auto entityInfo : entityInfoList)
        {
            std::string entityName;
            entityInfo.at("name").get_to(entityName);
            auto xformInfo = entityInfo.at("xform");
            Transform xform = entityInfo.at("xform").get<Transform>();
            Entity* entity = SceneManager::getSingletonPtr()->createEntity(scene, entityName.c_str(), xform);
            auto sceneNodes = entityInfo.at("nodes");
            for (auto node : sceneNodes)
            {
                entity->getSceneRoot()->attach(m_nodes[node]);
            }
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

        // create buffer size that can contain max number of lights
        scene->m_dirLightsBuffer = Cyan::createRegularBuffer(Scene::kMaxNumDirLights * sizeof(DirectionalLight));
        scene->m_pointLightsBuffer = Cyan::createRegularBuffer(Scene::kMaxNumPointLights * sizeof(PointLight));

        // TODO: each scene should only have one camera
        scene->activeCamera = 0u;
        u32 camIdx = 0u;
        for (auto& camera : cameras) 
        {
            scene->cameras[camIdx] = camera.get<Camera>();
            scene->cameras[camIdx].view = glm::mat4(1.f);
            CameraManager::updateCamera(scene->cameras[camIdx++]);
        }

        loadTextures(textureInfoList);
        loadMeshes(scene, meshInfoList);
        loadNodes(scene, nodes);
        loadEntities(scene, entities);
    }

    // TODO: Fix this, the loaded textures are all black!!!
    Cyan::Texture* AssetManager::loadGltfTexture(tinygltf::Model& model, i32 index) {
        using Cyan::Texture;
        auto textureManager = Cyan::TextureManager::getSingletonPtr();
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
                        spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                    } else if (image.bits == 16) {
                        spec.m_format = Cyan::Texture::ColorFormat::R16G16B16;
                        spec.m_dataType = Texture::DataType::Float;
                    } else if (image.bits == 32) {
                        CYAN_ASSERT(0, "R32G32B32 color format is currently not supported!")
                    } 
                    break;
                }
                case 4: {
                    if (image.bits == 8) {
                        spec.m_format = Cyan::Texture::ColorFormat::R8G8B8A8;
                        spec.m_dataType = Texture::DataType::UNSIGNED_BYTE;
                    } else if (image.bits == 16) {
                        spec.m_format = Cyan::Texture::ColorFormat::R16G16B16A16;
                        spec.m_dataType = Texture::DataType::Float;
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
                    texture = textureManager->createTexture(name, spec);
                    break;
                }
                case Texture::ColorFormat::R16G16B16:
                case Texture::ColorFormat::R16G16B16A16: {
                    texture = textureManager->createTextureHDR(name, spec);
                    break;
                }
            }
        }
        return texture;
    }

    // TODO: Normalize mesh scale
    SceneNode* AssetManager::loadGltfNode(Scene* scene, tinygltf::Model& model, tinygltf::Node* parent, 
                        SceneNode* parentSceneNode, tinygltf::Node& node, u32 numNodes) {
        auto textureManager = Cyan::TextureManager::getSingletonPtr();
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

        // create an entity for this node
        char sceneNodeName[64];
        CYAN_ASSERT(node.name.size() < kEntityNameMaxLen, "Entity name too long !!")
        if (node.name.empty()) {
            sprintf_s(sceneNodeName, "Node%u", numNodes);
        } else {
            sprintf_s(sceneNodeName, "%s", node.name.c_str());
        }

        Cyan::Mesh* mesh = hasMesh ? Cyan::getMesh(meshName) : nullptr;
        SceneNode* sceneNode = Cyan::createSceneNode(sceneNodeName, localTransform, mesh);
        if (parentSceneNode)
        {
            parentSceneNode->attach(sceneNode);
        }
        // bind material
        if (sceneNode->m_meshInstance) {
            auto& gltfMesh = model.meshes[node.mesh];
            for (u32 sm = 0u; sm < gltfMesh.primitives.size(); ++sm) {
                // !TODO: material is hard coded to pbr material for now 
                Shader* pbrShader = Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs", "../../shader/shader_pbr.fs");
                Cyan::MeshInstance* mesh = sceneNode->m_meshInstance;
                mesh->m_matls[sm] = Cyan::createMaterial(pbrShader)->createInstance();
                auto& primitive = gltfMesh.primitives[sm];
                if (primitive.material > -1) {
                    auto& gltfMaterial = model.materials[primitive.material];
                    auto pbr = gltfMaterial.pbrMetallicRoughness;
                    auto getTexture = [&](i32 imageIndex) {
                        Cyan::Texture* texture = nullptr;
                        if (imageIndex > -1) {
                            auto& image = model.images[imageIndex];
                            texture = textureManager->getTexture(image.uri.c_str());
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
                    mesh->m_matls[sm]->bindTexture("metallicRoughnessMap", metallicRoughness); 
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
                    mesh->m_matls[sm]->set("indirectDiffuseSlider", 0.0f);
                    mesh->m_matls[sm]->set("indirectSpecularSlider", 0.0f);
                    mesh->m_matls[sm]->set("wrap", 0.15f);
                }
            }
        }
        // recurse to load all the children
        for (auto& child : node.children) {
            loadGltfNode(scene, model, &node, sceneNode, model.nodes[child], ++numNodes);
        }
        return sceneNode;
    }

    Cyan::Mesh* AssetManager::loadGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh) {
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
        // mesh->m_normalization = glm::scale(glm::mat4(1.f), glm::vec3(0.01f, 0.01f, 0.01f));
        mesh->m_normalization = glm::mat4(1.0);
        mesh->m_shouldNormalize = false;
        mesh->onFinishLoading();
        return mesh;
    }

    void AssetManager::loadGltfTextures(tinygltf::Model& model) {
        using Cyan::Texture;
        for (u32 t = 0u; t < model.textures.size(); ++t) {
            loadGltfTexture(model, t);
        }
    }

    SceneNode* AssetManager::loadGltf(Scene* scene, const char* filename, const char* name, Transform transform)
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
        Cyan::Mesh* rootNodeMesh = nullptr;
        // TODO: handle transform of the root node
        if (rootNode.mesh > 0)
        {
            tinygltf::Mesh gltfMesh = model.meshes[rootNode.mesh];
            rootNodeMesh = Cyan::getMesh(gltfMesh.name.c_str());
        }
        SceneNode* parentNode = Cyan::createSceneNode(name, transform);
        loadGltfNode(scene, model, nullptr, parentNode, rootNode, 0);
        return parentNode;
    }
}
