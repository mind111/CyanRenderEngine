#include <fstream>
#include <thread>
#include <sstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <tiny_obj/tiny_obj_loader.h>

#include "AssetManager.h"
#include "Texture.h"
#include "CyanAPI.h"

namespace std 
{
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

namespace glm 
{
    void from_json(const nlohmann::json& j, glm::vec3& v) { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }

    void from_json(const nlohmann::json& j, glm::vec4& v)  { 
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
        v.w = j.at(3).get<float>();
    }
}

void from_json(const nlohmann::json& j, Transform& t) {
    t.m_translate = j.at("translation").get<glm::vec3>();
    glm::vec4 rotation = j.at("rotation").get<glm::vec4>();
    t.m_qRot = glm::quat(cos(RADIANS(rotation.x * 0.5f)), sin(RADIANS(rotation.x * 0.5f)) * glm::vec3(rotation.y, rotation.z, rotation.w));
    t.m_scale = j.at("scale").get<glm::vec3>();
}

namespace Cyan
{
    bool operator==(const Triangles::Vertex& lhs, const Triangles::Vertex& rhs) {
        // todo: maybe use memcmp() here instead ..?
        // bit equivalence
        return (lhs.pos == rhs.pos)
            && (lhs.normal == rhs.normal)
            && (lhs.tangent == rhs.tangent)
            && (lhs.texCoord0 == rhs.texCoord0);
    }

    AssetManager* AssetManager::singleton = nullptr;
    AssetManager::AssetManager() {
        if (!singleton) {
            singleton = this;
        }
    }

    void AssetManager::importTextures(const nlohmann::basic_json<std::map>& textureInfoList) {
        using Cyan::Texture2DRenderable;
        for (auto textureInfo : textureInfoList) {
            std::string filename = textureInfo.at("path").get<std::string>();
            std::string name = textureInfo.at("name").get<std::string>();
            std::string dynamicRange = textureInfo.at("dynamic_range").get<std::string>();
            u32 numMips = textureInfo.at("numMips").get<u32>();

            ITextureRenderable::Spec spec = { };
            spec.numMips = numMips;
            Texture2DRenderable* texture = nullptr;
            texture = importTexture2D(
                name.c_str(),
                filename.c_str(),
                spec,
                ITextureRenderable::Parameter{
                    ITextureRenderable::Parameter::Filtering::LINEAR,
                    ITextureRenderable::Parameter::Filtering::LINEAR,
                    ITextureRenderable::Parameter::WrapMode::WRAP,
                    ITextureRenderable::Parameter::WrapMode::WRAP,
                    ITextureRenderable::Parameter::WrapMode::WRAP
                });
        }
    }

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
    std::vector<ISubmesh*> AssetManager::importObj(const char* baseDir, const char* filename) {
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

                submeshes.push_back(createSubmesh<Lines>(vertices, indices));
            }
        }

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

    Mesh* AssetManager::importMesh(Scene* scene, std::string& path, const char* name, bool normalize) {
        Cyan::ScopedTimer meshTimer(name, true);

        // get extension from mesh path
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        found = path.find_last_of('/');
        std::string baseDir = path.substr(0, found);

        Mesh* parent = nullptr;
        if (extension == ".obj")
        {
            cyanInfo("Importing .obj file %s", path.c_str());
            std::vector<ISubmesh*> submeshes = std::move(importObj(baseDir.c_str(), path.c_str()));
            parent = createMesh(name, submeshes);
        }
        else if (extension == ".gltf")
        {
            cyanInfo("Importing .gltf file %s", path.c_str());
            importGltf(scene, path.c_str(), name);
        }
        else if (extension == ".glb")
        {
            cyanInfo("Importing .glb file %s", path.c_str());
            importGltf(scene, path.c_str(), name);
        }
        else
            cyanError("Unsupported mesh file format %s", extension.c_str());

        // todo: (handle converting mesh data from raster pipeline to ray tracing pipeline)
        // mesh->m_bvh  = nullptr;
        return parent;
    }

    void AssetManager::importMeshes(Scene* scene, const nlohmann::basic_json<std::map>& meshInfoList) {
        for (auto meshInfo : meshInfoList)
        {
            std::string path, name;
            bool normalize, generateLightMapUv;
            meshInfo.at("path").get_to(path);
            meshInfo.at("name").get_to(name);
            meshInfo.at("normalize").get_to(normalize);
            meshInfo.at("generateLightMapUv").get_to(generateLightMapUv);

            importMesh(scene, path, name.c_str(), normalize);
        }
    }

    void AssetManager::importEntities(Scene* scene, const nlohmann::basic_json<std::map>& entityInfoList)
    {
        Cyan::ScopedTimer timer("importEntities()", true);
        for (auto entityInfo : entityInfoList)
        {
            // name
            std::string entityName;
            entityInfo.at("name").get_to(entityName);
            // transform
            auto xformInfo = entityInfo.at("xform");
            Transform xform = entityInfo.at("xform").get<Transform>();
            // properties
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
            // mesh
            std::string meshName;
            auto meshInfo = entityInfo.at("mesh");
            meshInfo.get_to(meshName);
            if (meshName != "None")
            {
                Mesh* mesh = getAsset<Mesh>(meshName.c_str());
                if (mesh)
                {
                    scene->createStaticMeshEntity(entityName.c_str(), xform, mesh, nullptr, properties);
                }
                else
                {
                    cyanError("Mesh with name %s does not exist", meshName.c_str());
                }
            }
            else
            {
                Entity* entity = scene->createEntity(entityName.c_str(), xform, nullptr, properties);
                const auto& childInfoList = entityInfo.at("childs");
                for (const auto& childInfo : childInfoList)
                {
                    std::string childName;
                    childInfo.get_to(childName);
                    if (Entity* child = scene->getEntity(childName.c_str()))
                    {
                        entity->attach(child);
                    }
                    else
                    {
                        cyanError("Entity with name %s does not exist", childName.c_str());
                    }
                }
            }
        }
    }

    void AssetManager::importScene(Scene* scene, const char* file)
    {
        nlohmann::json sceneJson;
        std::ifstream sceneFile(file);
        try {
            sceneFile >> sceneJson;
        }
        catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
        }

        const auto& meshInfoList = sceneJson["meshes"];
        const auto& textureInfoList = sceneJson["textures"];
        const auto& entities = sceneJson["entities"];

        importTextures(textureInfoList);
        importMeshes(scene, meshInfoList);
        importEntities(scene, entities);
    }

    Texture2DRenderable* AssetManager::importGltfTexture(tinygltf::Model& model, tinygltf::Texture& gltfTexture)
    {
        Texture2DRenderable* texture = nullptr;

        tinygltf::Image& image = model.images[gltfTexture.source];
        const tinygltf::Sampler& gltfSampler = model.samplers[gltfTexture.sampler];

        Cyan::ITextureRenderable::Spec spec = { };
        spec.width = image.width;
        spec.height = image.height;
        switch (image.component)
        {
        case 3:
        {
            if (image.bits == 8)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB8;
            }
            else if (image.bits == 16)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB16F;
            }
            else if (image.bits == 32)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGB32F;
            }
            break;
        }
        case 4: {
            if (image.bits == 8)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGBA8;
            }
            else if (image.bits == 16)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGBA16F;
            }
            else if (image.bits == 32)
            {
                spec.pixelFormat = ITextureRenderable::Spec::PixelFormat::RGBA32F;
            }
            break;
        }
        default:
        {
            cyanError("Invalid number of pixel channels when loading gltf image");
            break;
        }
        }

        if (gltfSampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR)
        {
            spec.numMips = std::log2(min(image.width, image.height)) + 1;
        }
        spec.pixelData = reinterpret_cast<u8*>(image.image.data());

        ITextureRenderable::Parameter parameter = { };
        if (gltfTexture.sampler >= 0)
        {
            const auto& sampler = model.samplers[gltfTexture.sampler];
            switch (sampler.minFilter)
            {
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                parameter.minificationFilter = FM_BILINEAR;
                break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                parameter.minificationFilter = FM_POINT;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                parameter.minificationFilter = FM_TRILINEAR;
                break;
            default:
                break;
            }
            switch (sampler.magFilter)
            {
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                parameter.magnificationFilter = FM_BILINEAR;
                break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                parameter.magnificationFilter = FM_POINT;
                break;
            default:
                break;
            }
            switch (sampler.wrapS)
            {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                parameter.wrap_s = WM_CLAMP;
                break;
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                parameter.wrap_s = WM_WRAP;
                break;
            default:
                break;
            }
            switch (sampler.wrapT)
            {
            case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
                parameter.wrap_t = WM_CLAMP;
                break;
            case TINYGLTF_TEXTURE_WRAP_REPEAT:
                parameter.wrap_t = WM_WRAP;
                break;
            default:
                break;
            }
        }

        std::string textureName;
        if (!gltfTexture.name.empty())
        {
            textureName = gltfTexture.name;
        }
        else
        {
            textureName = std::string(image.name);
        }
        texture = createTexture2D(textureName.c_str(), spec, parameter);
        return texture;
    }

    void AssetManager::importGltfNode(Scene* scene, tinygltf::Model& model, Entity* parent, tinygltf::Node& node)
    {
        bool isCamera = (node.camera > -1);
        bool hasMesh = (node.mesh > -1);
        bool hasSkin = (node.skin > -1);
        bool hasMatrix = !node.matrix.empty();
        bool hasTransform = ((!node.rotation.empty() || !node.translation.empty() || !node.scale.empty()) || hasMatrix);
        const char* meshName = hasMesh ? model.meshes[node.mesh].name.c_str() : nullptr;
        Transform localTransform;
        if (hasMatrix)
        {
            glm::mat4 matrix =
            {
                glm::vec4(node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3]),     // column 0
                glm::vec4(node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7]),     // column 1
                glm::vec4(node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11]),   // column 2
                glm::vec4(node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15])  // column 3
            };
            // convert matrix to local transform
            localTransform.fromMatrix(matrix);
        }
        else if (hasTransform)
        {
            if (!node.translation.empty())
            {
                localTransform.m_translate = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }
            if (!node.scale.empty())
            {
                localTransform.m_scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }
            if (!node.rotation.empty())
            {
                // glm quaternion constructor (w, x, y, z) while gltf (x, y, z, w)
                localTransform.m_qRot = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
            }
        }

        const char* entityName = nullptr;
        if (!node.name.empty())
        {
            entityName = node.name.c_str();
        }

        Entity* entity = nullptr;
        // todo: import camera & light directly from gltf
        if (isCamera)
        {
#if 0
            const tinygltf::Camera& gltfCamera = model.cameras[node.camera];
            if (gltfCamera.type == "perspective")
            {
                auto camera = scene->createPerspectiveCamera(
                    node.name.c_str(),
                    localTransform,
                    glm::vec3(0.f, 1.f, 0.f),
                    gltfCamera.perspective.yfov,
                    gltfCamera.perspective.znear,
                    gltfCamera.perspective.zfar,
                    gltfCamera.perspective.aspectRatio);
                gltfCamera.perspective.
            }
            else if (gltfCamera.type == "orthographic")
            {
                cyanError("orthographics camera is currently not supported");
            }
#endif
        }

        if (hasMesh)
        {
            Mesh* mesh = getAsset<Mesh>(meshName);
            auto staticMeshEntity = scene->createStaticMeshEntity(entityName, localTransform, mesh, parent);
            entity = staticMeshEntity;

            // setup material
            auto& gltfMesh = model.meshes[node.mesh];
            for (u32 sm = 0u; sm < gltfMesh.primitives.size(); ++sm)
            {
                std::string matlName("DefaultMaterial");

#if 0
                auto& primitive = gltfMesh.primitives[sm];
                if (primitive.material > -1)
                {
                    auto getTexture = [&](i32 imageIndex)
                    {
                        Cyan::Texture2DRenderable* texture = nullptr;
                        if (imageIndex > -1 && imageIndex < model.images.size())
                        {
                            auto& image = model.images[imageIndex];
                            texture = getAsset<Texture2DRenderable>(image.name.c_str());
                            if (!texture)
                            {
                                texture = getAsset<Texture2DRenderable>(image.uri.c_str());
                            }
                        }
                        return texture;
                    };

                    auto& gltfMaterial = model.materials[primitive.material];
                    auto pbr = gltfMaterial.pbrMetallicRoughness;
                    if (gltfMaterial.name.empty())
                    {
                        matlName = std::string("material_") + std::to_string(primitive.material);
                    }
                    else
                    {
                        matlName = gltfMaterial.name;
                    }
                    Material& matl = createMaterial(matlName.c_str());
                    matl.albedoMap = getTexture(pbr.baseColorTexture.index);
                    matl.normalMap = getTexture(gltfMaterial.normalTexture.index);
                    matl.metallicRoughnessMap = getTexture(pbr.metallicRoughnessTexture.index);
                    matl.occlusionMap = getTexture(gltfMaterial.occlusionTexture.index);
                    matl.albedo = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
                    matl.roughness = pbr.roughnessFactor;
                    matl.metallic = pbr.metallicFactor;
                    glm::vec3 emissiveColor(gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1], gltfMaterial.emissiveFactor[2]);
                    f32 emissiveIntensity = glm::length(emissiveColor);
                    if (emissiveIntensity > 0.f) {
                        matl.albedo = glm::vec4(glm::normalize(emissiveColor), 1.f);
                        matl.emissive = emissiveIntensity;
                    }
                }
#endif
                staticMeshEntity->setMaterial(getAsset<Material>(matlName.c_str()), sm);
            }
        }
        else
        {
            entity = scene->createEntity(entityName, localTransform, parent);
        }
        // recurse to load all the children
        for (auto& child : node.children)
        {
            importGltfNode(scene, model, entity, model.nodes[child]);
        }
    }

    void AssetManager::importGltfAsync(Scene* scene, const char* filename)
    {
        u32 strLength = strlen(filename) + strlen("Importing gltf ");
        u32 bufferSize = strLength + 1;
        char* str = static_cast<char*>(alloca(bufferSize));
        sprintf_s(str, bufferSize, "Importing gltf %s", filename);
        cyanInfo("Importing gltf %s", str);

        tinygltf::Model model;
        std::string warn, err;

        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        found = path.find_last_of('/');
        std::string baseDir = path.substr(0, found);

        if (extension == ".gltf")
        {
            if (!singleton->m_gltfImporter.LoadASCIIFromFile(&model, &err, &warn, std::string(filename)))
            {
                std::cout << warn << std::endl;
                std::cout << err << std::endl;
            }
        }
        else if (extension == ".glb")
        {
            ScopedTimer timer("Import .glb timer", true);
            if (!singleton->m_gltfImporter.LoadBinaryFromFile(&model, &err, &warn, std::string(filename)))
            {
                std::cout << warn << std::endl;
                std::cout << err << std::endl;
            }
        }

        // synchronously import all the entities in the scene first
        tinygltf::Scene& gltfScene = model.scenes[model.defaultScene];
        auto importNode = [](Scene* scene, tinygltf::Model& model, Entity* parent, tinygltf::Node& node) {
            bool isCamera = (node.camera > -1);
            bool hasMesh = (node.mesh > -1);
            bool hasSkin = (node.skin > -1);
            bool hasMatrix = !node.matrix.empty();
            bool hasTransform = ((!node.rotation.empty() || !node.translation.empty() || !node.scale.empty()) || hasMatrix);
            const char* meshName = hasMesh ? model.meshes[node.mesh].name.c_str() : nullptr;
            Transform localTransform;
            if (hasMatrix)
            {
                glm::mat4 matrix =
                {
                    glm::vec4(node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3]),     // column 0
                    glm::vec4(node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7]),     // column 1
                    glm::vec4(node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11]),   // column 2
                    glm::vec4(node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15])  // column 3
                };
                // convert matrix to local transform
                localTransform.fromMatrix(matrix);
            }
            else if (hasTransform)
            {
                if (!node.translation.empty())
                {
                    localTransform.m_translate = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
                }
                if (!node.scale.empty())
                {
                    localTransform.m_scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
                }
                if (!node.rotation.empty())
                {
                    // glm quaternion constructor (w, x, y, z) while gltf (x, y, z, w)
                    localTransform.m_qRot = glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                }
            }

            const char* entityName = nullptr;
            if (!node.name.empty())
            {
                entityName = node.name.c_str();
            }
            Entity* entity = nullptr;
            if (hasMesh)
            {
                assert(meshName);
                Mesh* mesh = getAsset<Mesh>(meshName);
                if (mesh == nullptr)
                {
                    // create an empty mesh
                    Mesh* mesh = createMesh(meshName);
                    // push it into pending load queue
                    // todo: have a worker thread watch this queue and automatically dispatch loading task
                    // todo: worry about materials
                    // pendingLoadMeshes.push({ mesh, model.meshes[node.mesh] });
                }
                scene->createStaticMeshEntity(entityName, localTransform, mesh, parent);
            }
            else
            {
                scene->createEntity(entityName, localTransform, parent);
            }
        };

        // import gltf nodes
        for (u32 i = 0; i < gltfScene.nodes.size(); ++i)
        {
            importNode(scene, model, scene->m_rootEntity, model.nodes[gltfScene.nodes[i]]);
        }

        auto asyncImportMesh = []() {

        };

        // once all the nodes in the scene hierarchy are imported, loading assets in another thread asynchronously 
        std::thread asyncLoad(asyncImportMesh);
    }

    // json parsing helpers
    using json = nlohmann::json;
    using json_iterator = nlohmann::json::iterator;
    using json_const_iterator = nlohmann::json::const_iterator;

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

    bool jsonFindAndGetVec3(glm::vec3& outVec3, const json& o, const char* name)
    {
        json_const_iterator itr;
        if (jsonFind(itr, o, name))
        {
            return jsonGetVec3(outVec3, itr.value());
        }
    }

    bool jsonFindAndGetBool(bool& outBool, const json& o, const char* name)
    {
        json_const_iterator itr;
        if (jsonFind(itr, o, name))
        {
            return jsonGetBool(outBool, itr.value());
        }
    }

    bool jsonFindAndGetVec4(glm::vec4& outVec3, const json& o, const char* name)
    {
        json_const_iterator itr;
        if (jsonFind(itr, o, name))
        {
            return jsonGetVec4(outVec3, itr.value());
        }
    }

    bool jsonFindAndGetInt(i32& outInt, const json& o, const char* name)
    {
        json_const_iterator itr;
        if (jsonFind(itr, o, name))
        {
            return jsonGetInt(outInt, itr.value());
        }
    }

    bool jsonFindAndGetUint(u32& outUint, const json& o, const char* name)
    {
        json_const_iterator itr;
        if (jsonFind(itr, o, name))
        {
            return jsonGetUint(outUint, itr.value());
        }
    }

    bool jsonFindAndGetString(std::string& outStr, const json& o, const char* name)
    {
        json_const_iterator itr;
        if (jsonFind(itr, o, name))
        {
            return jsonGetString(outStr, itr.value());
        }
    }

    void parseNode(Scene* scene, Entity* parent, u32 node, const json& nodes)
    {
        const json& o = nodes[node];

        // @name
        std::string name;
        jsonFindAndGetString(name, o, "name");
        // @transform
        Transform t;
        json_const_iterator matrixItr;
        if (jsonFind(matrixItr, o, "matrix"))
        {
            const json& mo = matrixItr.value();
            if (mo.is_array())
            {
                std::array<float, 16> m;
                mo.get_to(m);
                glm::mat4 mat = {
                    glm::vec4(m[0],  m[1],  m[2],  m[3]),     // column 0
                    glm::vec4(m[4],  m[5],  m[6],  m[7]),     // column 1
                    glm::vec4(m[8],  m[9],  m[10], m[11]),    // column 2
                    glm::vec4(m[12], m[13], m[14], m[15])     // column 3
                };
                t.fromMatrix(mat);
            }
        }
        else
        {
            glm::vec3 scale(1.f); glm::vec4 rotation(0.f, 0.f, 0.f, 1.f); glm::vec3 translation(0.f);
            // @scale
            jsonFindAndGetVec3(scale, o, "scale");
            // @rotation
            jsonFindAndGetVec4(rotation, o, "rotation");
            // @translation
            jsonFindAndGetVec3(translation, o, "translation");
            t.m_scale = scale;
            t.m_qRot = glm::quat(rotation.w, glm::vec3(rotation.x, rotation.y, rotation.z));
            t.m_translate = translation;
        }
        // @mesh
        u32 mesh;
        bool bHasMesh = jsonFindAndGetUint(mesh, o, "mesh");
        Entity* e = scene->createEntity(name.c_str(), t, parent);

        // recurse into children nodes
        std::vector<u32> children;
        json_const_iterator itr = o.find("children");
        if (itr != o.end())
        {
            itr.value().get_to(children);
        }
        for (auto child : children)
        {
            parseNode(scene, e, child, nodes);
        }
    }

    namespace gltf
    {
        struct Scene
        {
            std::string name;
            std::vector<u32> nodes;
        };

        struct Node
        {
            // required
            std::string name;
            // optional
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
            Attribute attribute;
            i32 indices = -1;
            i32 material = -1;
            i32 mode = 4; // per gltf-2.0 spec, 4 corresponds to triangles
        };

        struct Mesh
        {
            // required
            std::vector<Primitive> primitives;
            // optional
            std::string name;
        };

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

        // abstraction of the parsed json content of a .glb file
        struct Descriptor
        {
            struct ChunkDesc
            {
                u32 chunkLength;
                u32 chunkType;
            };

            Descriptor(const char* inFilename, const ChunkDesc& inJsonChunkDesc, const ChunkDesc& inBinaryChunkDesc, u32 binaryChunkOffset, const json& inGltfJsonObject)
                : filename(inFilename), jsonChunkDesc(inJsonChunkDesc), binaryChunkDesc(inBinaryChunkDesc), o(inGltfJsonObject)
            {
            }

            ~Descriptor()
            {
            }

            void initialize()
            {
                if (!bInitailized)
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
                        }
                    }
                }
            }

            bool bInitailized = false;
            const char* filename = nullptr;
            ChunkDesc jsonChunkDesc;
            ChunkDesc binaryChunkDesc;
            u32 binaryChunkOffset;
            // json object parsed from raw json string
            json o;
            u32 defaultScene = -1;
            std::vector<Scene> scenes;
            std::vector<Node> nodes;
            std::vector<Mesh> meshes;
            std::vector<Accessor> accessors;
            std::vector<Buffer> buffers;
            std::vector<BufferView> bufferViews;
        };
    }

    // todo: implement this!!!
    /** note - @min:
    * first load and parse the json part, 
    * only load in geometry data first, worry about materials later
    */
    void AssetManager::importGltfEx(Scene* scene, const char* filename)
    {
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        found = path.find_last_of('/');
        std::string baseDir = path.substr(0, found);

        if (extension == ".gltf")
        {
        }
        else if (extension == ".glb")
        {
            ScopedTimer timer("Custom import .glb timer", true);
            std::ifstream glb(filename, std::ios::binary);
            if (!glb.is_open())
            {
                cyanError("Fail to open file: %s", filename);
                return;
            }
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
            if (header.magic != 0x46546C67)
            {
                cyanError("Invalid magic number found in the header for .glb file %s", filename);
            }
            // read-in & parse the json chunk (reading this part as a whole could be slow ...?)
            using ChunkDesc = gltf::Descriptor::ChunkDesc;
            ChunkDesc jsonChunkDesc;
            glb.read(reinterpret_cast<char*>(&jsonChunkDesc), sizeof(ChunkDesc));
            if (jsonChunkDesc.chunkType != 0x4E4F534A)
            {
                return;
            }
            // todo: will this part be slow? should the json part also be streamed in...?
            std::string jsonStr(" ", jsonChunkDesc.chunkLength);
            glb.read(&jsonStr[0], jsonChunkDesc.chunkLength);
            gltf::Descriptor::ChunkDesc binaryChunkDesc;
            glb.read(reinterpret_cast<char*>(&binaryChunkDesc), sizeof(ChunkDesc));
            u32 binaryChunkOffset = glb.tellg();

            // parse the json using json.hpp
            json o = nlohmann::json::parse(jsonStr);
            gltf::Descriptor desc(filename, jsonChunkDesc, binaryChunkDesc, binaryChunkOffset, o);
            // asynchronously stream in the binary chunk ...?
            struct StreamGltfBinaryTask
            {
                StreamGltfBinaryTask(const gltf::Descriptor& inDesc)
                    : desc(inDesc)
                {
                }

                void operator()()
                {
                    // open the file, and start reading from the binary chunk
                    std::ifstream glb(filename, std::ios::binary);
                    if (glb.is_open())
                    {
                        glb.seekg(binaryChunkOffset);

                        // 1. load all bufferViews
                        json jBufferViews;
                        if (jsonFind(jBufferViews, gltf, "bufferViews"))
                        {
                            u32 numBufferViews = jBufferViews.size();
                            bufferViews.resize(numBufferViews);
                            for (i32 i = 0; i < numBufferViews; ++i)
                            {
                                jBufferViews[i].get_to(bufferViews[i]);
                            }
                        }
                        // 2. load all accessors
                        json jAccessors;
                        if (jsonFind(jAccessors, gltf, "accessors"))
                        {
                            u32 numAccessors = jAccessors.size();
                            accessors.resize(numAccessors);
                            for (i32 i = 0; i < numAccessors; ++i)
                            {
                                jAccessors[i].get_to(accessors[i]);
                            }
                        }
                        // 3. load all buffers (for .glb there should be only one buffer)
                        json jBuffers;
                        if (jsonFind(jBuffers, gltf, "buffers"))
                        {
                            u32 numBuffers = jBuffers.size();
                            buffers.resize(numBuffers);
                            for (i32 i = 0; i < numBuffers; ++i)
                            {
                                jBuffers[i].get_to(buffers[i]);
                            }
                        }
                        // 4. load the buffer from disk into memory assuming that it all fit
                        binaryChunk.resize(binaryChunkDesc.chunkLength);
                        // todo: this is a slow operation depending on size of the chunk to read in
                        glb.read(reinterpret_cast<char*>(binaryChunk.data()), binaryChunkDesc.chunkLength);
                        // 5. parse mesh data from loaded binary chunk
                        json jMeshes;
                        jsonFind(jMeshes, gltf, "meshes");
                        if (jMeshes.is_array())
                        {
                            u32 numMeshes = jMeshes.size();
                            meshes.resize(numMeshes);
                            for (i32 m = 0; m < numMeshes; ++m)
                            {
                                jMeshes[m].get_to(meshes[m]);
                                i32 numSubmeshes = meshes[m].primitives.size();
                                std::vector<ISubmesh*> submeshes(numSubmeshes);
                                for (i32 sm = 0; sm < numSubmeshes; ++sm)
                                {
                                    const gltf::Primitive& p = meshes[m].primitives[sm];
                                    u32 numVertices = accessors[p.attribute.position].count;
                                    std::vector<Triangles::Vertex> vertices(numVertices);
                                    std::vector<u32> indices;

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
                                                vertices[v].texCoord0 = texCoord0[v];
                                            }
                                        }

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
                                    submeshes[sm] = createSubmesh<Triangles>(vertices, indices);
                                }
                                createMesh(meshes[m].name.c_str(), submeshes);
                            }
                        }
                    }
                }

                json gltf;
                const char* filename = nullptr;
                ChunkDesc binaryChunkDesc;
                u32 binaryChunkOffset;

                std::vector<u8> binaryChunk;
                std::vector<gltf::Buffer> buffers;
                std::vector<gltf::BufferView> bufferViews;
                std::vector<gltf::Accessor> accessors;
                std::vector<gltf::Mesh> meshes;
            };

            StreamGltfBinaryTask task(gltf, filename, binaryChunkDesc, binaryChunkOffset);
            task();
        }
    }
#if 0
                    // 1. parse "scenes"
                    std::vector<gltf::Scene> scenes;
                    json jScenes;
                    if (jsonFind(jScenes, gltf, "scenes"))
                    {
                        if (jScenes.is_array())
                        {
                            u32 numScenes = jScenes.size();
                            scenes.resize(numScenes);
                            for (i32 i = 0; i < numScenes; ++i)
                            {
                                gltf::Scene& scene = scenes[i];
                                jScenes[i].get_to(scene);
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
                    u32 defaultScene = -1;
                    json jDefaultScene;
                    if (jsonFind(jDefaultScene, gltf, "scene"))
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

                    // cache the binary chunk info
                    ChunkDesc binaryChunkDesc;
                    glb.read(reinterpret_cast<char*>(&binaryChunkDesc), sizeof(ChunkDesc));
                    u32 binaryChunkOffset = glb.tellg();
                    glb.close();

                    // asynchronously stream in the binary chunk ...?
                    struct StreamGltfBinaryTask
                    {
                        StreamGltfBinaryTask(const json& o, const char* inFilename, const ChunkDesc& desc, u32 offset)
                            : gltf(o), filename(inFilename), binaryChunkDesc(desc), binaryChunkOffset(offset)
                        {
                        }

                        void operator()()
                        {
                            // open the file, and start reading from the binary chunk
                            std::ifstream glb(filename, std::ios::binary);
                            if (glb.is_open())
                            {
                                glb.seekg(binaryChunkOffset);

                                // 1. load all bufferViews
                                json jBufferViews;
                                if (jsonFind(jBufferViews, gltf, "bufferViews"))
                                {
                                    u32 numBufferViews = jBufferViews.size();
                                    bufferViews.resize(numBufferViews);
                                    for (i32 i = 0; i < numBufferViews; ++i)
                                    {
                                        jBufferViews[i].get_to(bufferViews[i]);
                                    }
                                }
                                // 2. load all accessors
                                json jAccessors;
                                if (jsonFind(jAccessors, gltf, "accessors"))
                                {
                                    u32 numAccessors = jAccessors.size();
                                    accessors.resize(numAccessors);
                                    for (i32 i = 0; i < numAccessors; ++i)
                                    {
                                        jAccessors[i].get_to(accessors[i]);
                                    }
                                }
                                // 3. load all buffers (for .glb there should be only one buffer)
                                json jBuffers;
                                if (jsonFind(jBuffers, gltf, "buffers"))
                                {
                                    u32 numBuffers = jBuffers.size();
                                    buffers.resize(numBuffers);
                                    for (i32 i = 0; i < numBuffers; ++i)
                                    {
                                        jBuffers[i].get_to(buffers[i]);
                                    }
                                }
                                // 4. load the buffer from disk into memory assuming that it all fit
                                binaryChunk.resize(binaryChunkDesc.chunkLength);
                                // todo: this is a slow operation depending on size of the chunk to read in
                                glb.read(reinterpret_cast<char*>(binaryChunk.data()), binaryChunkDesc.chunkLength);
                                // 5. parse mesh data from loaded binary chunk
                                json jMeshes;
                                jsonFind(jMeshes, gltf, "meshes");
                                if (jMeshes.is_array())
                                {
                                    u32 numMeshes = jMeshes.size();
                                    meshes.resize(numMeshes);
                                    for (i32 m = 0; m < numMeshes; ++m)
                                    {
                                        jMeshes[m].get_to(meshes[m]);
                                        i32 numSubmeshes = meshes[m].primitives.size();
                                        std::vector<ISubmesh*> submeshes(numSubmeshes);
                                        for (i32 sm = 0; sm < numSubmeshes; ++sm)
                                        {
                                            const gltf::Primitive& p = meshes[m].primitives[sm];
                                            u32 numVertices = accessors[p.attribute.position].count;
                                            std::vector<Triangles::Vertex> vertices(numVertices);
                                            std::vector<u32> indices;

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
                                                        vertices[v].texCoord0 = texCoord0[v];
                                                    }
                                                }

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
                                            submeshes[sm] = createSubmesh<Triangles>(vertices, indices);
                                        }
                                        createMesh(meshes[m].name.c_str(), submeshes);
                                    }
                                }
                            }
                        }

                        json gltf;
                        const char* filename = nullptr;
                        ChunkDesc binaryChunkDesc;
                        u32 binaryChunkOffset;

                        std::vector<Byte> binaryChunk;
                        std::vector<gltf::Buffer> buffers;
                        std::vector<gltf::BufferView> bufferViews;
                        std::vector<gltf::Accessor> accessors;
                        std::vector<gltf::Mesh> meshes;
                    };

                    StreamGltfBinaryTask task(gltf, filename, binaryChunkDesc, binaryChunkOffset);
                    task();

                    json jSceneNodes;
                    jsonFind(jSceneNodes, gltf, "nodes");
                    // 3. parse the default "scene" node hierarchy given a set of root nodes
                    if (defaultScene != -1)
                    {
                        gltf::Scene& gltfScene = scenes[defaultScene];
                        std::vector<u32>& sceneRoots = gltfScene.nodes;
                        for (u32 root : sceneRoots)
                        {
                            parseNode(scene, nullptr, root, jSceneNodes);
                        }
                    }
                    else
                    {
                    }
#endif

    template <typename T>
    void loadVerticesAndIndices(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<typename T::Vertex>& vertices, std::vector<u32>& indices) {
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

    Cyan::Mesh* AssetManager::importGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh) 
    {
        std::vector<ISubmesh*> submeshes;

        // todo: multi-threading one thread per mesh
        // primitives (submeshes)
        for (u32 i = 0u; i < (u32)gltfMesh.primitives.size(); ++i)
        {
            tinygltf::Primitive& primitive = gltfMesh.primitives[i];

            switch (primitive.mode)
            {
            case TINYGLTF_MODE_TRIANGLES:
            {
                u32 numVertices = model.accessors[primitive.attributes.begin()->second].count;
                std::vector<Triangles::Vertex> vertices(numVertices);
                glm::vec3* positions = nullptr;
                glm::vec3* normals = nullptr;
                glm::vec4* tangents = nullptr;
                glm::vec2* texCoords = nullptr;

                auto getAttributeAddress = [&model, &primitive](const char* attributeName, i32 attributeType) {
                    u8* address = nullptr;
                    auto entry = primitive.attributes.find(attributeName);
                    if (entry != primitive.attributes.end())
                    {
                        i32 index = primitive.attributes[attributeName];
                        tinygltf::Accessor& accessor = model.accessors[index];
                        CYAN_ASSERT(accessor.type == attributeType, "Vertex attribute format is not supported!");
                        tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                        tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                        u32 byteOffset = accessor.byteOffset + bufferView.byteOffset;
                        address = buffer.data.data() + byteOffset;
                    }
                    return address;
                };

                positions = reinterpret_cast<glm::vec3*>(getAttributeAddress("POSITION", TINYGLTF_TYPE_VEC3));
                normals = reinterpret_cast<glm::vec3*>(getAttributeAddress("NORMAL", TINYGLTF_TYPE_VEC3));
                tangents = reinterpret_cast<glm::vec4*>(getAttributeAddress("TANGENT", TINYGLTF_TYPE_VEC4));
                texCoords = reinterpret_cast<glm::vec2*>(getAttributeAddress("TEXCOORD_0", TINYGLTF_TYPE_VEC2));

                if (positions)
                {
                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        vertices[v].pos = positions[v];
                    }
                }
                if (normals)
                {
                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        vertices[v].normal = normals[v];
                    }
                }
                if (tangents)
                {
                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        vertices[v].tangent = tangents[v];
                    }
                }
                if (texCoords)
                {
                    for (u32 v = 0; v < vertices.size(); ++v)
                    {
                        vertices[v].texCoord0 = glm::vec2(texCoords[v].x, 1.f - texCoords[v].y);
                    }
                }

                std::vector<u32> indices;
                if (primitive.indices >= 0)
                {
                    auto& accessor = model.accessors[primitive.indices];
                    u32 numIndices = accessor.count;
                    CYAN_ASSERT(numIndices % 3 == 0, "Invalid index count for triangle mesh")

                    indices.resize(numIndices);
                    tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                    u32 srcIndexSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
                    switch (srcIndexSize)
                    {
                    case 1:
                        for (u32 i = 0; i < numIndices; ++i)
                        {
                            u8 index = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i];
                            indices[i] = index;
                        }
                        break;
                    case 2:
                        for (u32 i = 0; i < numIndices; ++i)
                        {
                            u8 byte0 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 0];
                            u8 byte1 = buffer.data[accessor.byteOffset + bufferView.byteOffset + srcIndexSize * i + 1];
                            u32 index = (byte1 << 8) | byte0;
                            indices[i] = index;
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
                            indices[i] = index;
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
        return parent;
    }

    void AssetManager::importGltfTextures(tinygltf::Model& model) {
        for (u32 i = 0u; i < model.textures.size(); ++i) 
        {
            importGltfTexture(model, model.textures[i]);
        }
    }

    void AssetManager::importGltf(Scene* scene, const char* filename, const char* name)
    {
        u32 strLength = strlen(filename) + strlen("Importing gltf ");
        u32 bufferSize = strLength + 1;
        char* str = static_cast<char*>(alloca(bufferSize));
        sprintf_s(str, bufferSize, "Importing gltf %s", filename);
        cyanInfo("Importing gltf %s", str);

        tinygltf::Model model;
        std::string warn, err;

        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        found = path.find_last_of('/');
        std::string baseDir = path.substr(0, found);

        if (extension == ".gltf")
        {
            if (!singleton->m_gltfImporter.LoadASCIIFromFile(&model, &err, &warn, std::string(filename)))
            {
                std::cout << warn << std::endl;
                std::cout << err << std::endl;
            }
        }
        else if (extension == ".glb")
        {
            ScopedTimer timer("Import .glb timer", true);
            if (!singleton->m_gltfImporter.LoadBinaryFromFile(&model, &err, &warn, std::string(filename)))
            {
                std::cout << warn << std::endl;
                std::cout << err << std::endl;
            }
        }

        tinygltf::Scene& gltfScene = model.scenes[model.defaultScene];
#if 0
        // import textures
        {
            ScopedTimer textureImportTimer("gltf texture import timer", true);
            singleton->importGltfTextures(model);
        }
#endif
        // import meshes
        {
            ScopedTimer meshImportTimer("gltf mesh import timer", true);
            for (auto& gltfMesh : model.meshes)
            {
                singleton->importGltfMesh(model, gltfMesh);
            }
        }
        // import gltf nodes
        for (u32 i = 0; i < gltfScene.nodes.size(); ++i)
        {
            ScopedTimer entityImportTimer("gltf scene hierarchy import timer", true);
            singleton->importGltfNode(scene, model, scene->m_rootEntity, model.nodes[gltfScene.nodes[i]]);
        }
    }
}