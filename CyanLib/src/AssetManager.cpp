#include <fstream>
#include <thread>
#include <sstream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <tiny_obj/tiny_obj_loader.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "AssetManager.h"
#include "AssetImporter.h"
#include "Texture.h"
#include "CyanAPI.h"
#include "gltf.h"
#include "CyanRenderer.h"

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

    AssetManager* Singleton<AssetManager>::singleton = nullptr;
    AssetManager::AssetManager()
    {
        const u32 sizeInPixels = 16 * 1024;
        for (i32 i = 0; i < (i32)Texture2DAtlas::Format::kCount; ++i)
        {
            switch ((Texture2DAtlas::Format)(i)) 
            {
            case Texture2DAtlas::Format::kR8:
                atlases[i] = new Texture2DAtlas(sizeInPixels, PF_R8);
                break;
            case Texture2DAtlas::Format::kRGB8:
                atlases[i] = new Texture2DAtlas(sizeInPixels, PF_RGB8);
                break;
            case Texture2DAtlas::Format::kRGBA8:
                atlases[i] = new Texture2DAtlas(sizeInPixels, PF_RGBA8);
                break;
#if 0
            case Format::kRGB16F:
                assert(0);
                break;
            case Format::kRGBA16F:
                assert(0);
                break;
#endif
            default: 
                assert(0);
                break;
            }
        }
    }

    void AssetManager::initialize() 
    {
        /**
            initialize default geometries and shapes
        */ 
        // line

        // shared global quad mesh
        {
            float quadVerts[24] = {
                -1.f, -1.f, 0.f, 0.f,
                 1.f,  1.f, 1.f, 1.f,
                -1.f,  1.f, 0.f, 1.f,

                -1.f, -1.f, 0.f, 0.f,
                 1.f, -1.f, 1.f, 0.f,
                 1.f,  1.f, 1.f, 1.f
            };

            m_defaultShapes.fullscreenQuad = createStaticMesh("FullScreenQuadMesh");
            std::vector<Triangles::Vertex> vertices(6);
            vertices[0].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[0].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[1].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[1].texCoord0 = glm::vec2(1.f, 1.f);
            vertices[2].pos = glm::vec3(-1.f,  1.f, 0.f); vertices[2].texCoord0 = glm::vec2(0.f, 1.f);
            vertices[3].pos = glm::vec3(-1.f, -1.f, 0.f); vertices[3].texCoord0 = glm::vec2(0.f, 0.f);
            vertices[4].pos = glm::vec3( 1.f, -1.f, 0.f); vertices[4].texCoord0 = glm::vec2(1.f, 0.f);
            vertices[5].pos = glm::vec3( 1.f,  1.f, 0.f); vertices[5].texCoord0 = glm::vec2(1.f, 1.f);
            std::vector<u32> indices({ 0, 1, 2, 3, 4, 5 });
            Triangles* t = new Triangles(vertices, indices);
            m_defaultShapes.fullscreenQuad->addSubmeshImmediate(t);
        }

        // cube
        m_defaultShapes.unitCubeMesh = createStaticMesh("UnitCubeMesh");
        u32 numVertices = sizeof(cubeVertices) / sizeof(glm::vec3);
        std::vector<Triangles::Vertex> vertices(numVertices);
        std::vector<u32> indices(numVertices);
        for (u32 v = 0; v < numVertices; ++v)
        {
            vertices[v].pos = glm::vec3(cubeVertices[v * 3 + 0], cubeVertices[v * 3 + 1], cubeVertices[v * 3 + 2]);
            indices[v] = v;
        }
        m_defaultShapes.unitCubeMesh->addSubmeshImmediate(new Triangles(vertices, indices));

#if 0
        // quad
        m_defaultShapes.quad = importWavefrontObj("Quad", ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/quad.obj");
        // sphere
        m_defaultShapes.sphere = importWavefrontObj("Sphere", ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/sphere.obj");
        // icosphere
        m_defaultShapes.icosphere = importWavefrontObj("IcoSphere", ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/icosphere.obj");
        // bounding sphere
        // todo: line mesh doesn't work
        m_defaultShapes.boundingSphere = importWavefrontObj("BoundingSphere", ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/bounding_sphere.obj");
        // disk
        m_defaultShapes.disk = importWavefrontObj("Disk", ASSET_PATH "mesh/default/", ASSET_PATH "mesh/default/disk.obj");
        // todo: cylinder
#endif
 
        /**
        *   initialize default textures
        */ 
        {
            Sampler2D sampler = { };
            sampler.minFilter = FM_TRILINEAR;
            sampler.magFilter = FM_BILINEAR;
            sampler.wrapS = Sampler::Addressing::kWrap;
            sampler.wrapT = Sampler::Addressing::kWrap;

            auto checkerDarkImage = AssetImporter::importImageSync("default_checker_dark", ASSET_PATH "textures/defaults/checker_dark.png");
            m_defaultTextures.checkerDark = createTexture2D("default_checker_dark", checkerDarkImage, sampler);
            auto gridDarkImage = AssetImporter::importImageSync("default_grid_dark", ASSET_PATH "textures/defaults/grid_dark.png");
            m_defaultTextures.gridDark = createTexture2D("default_grid_dark", gridDarkImage, sampler);
        }
        {
            Sampler2D sampler = { };
            sampler.minFilter = FM_POINT;
            sampler.magFilter = FM_POINT;
            sampler.wrapS = WM_WRAP;
            sampler.wrapT = WM_WRAP;

            auto blueNoiseImage_128x128 = AssetImporter::importImageSync("BlueNoise_128x128_R", ASSET_PATH "textures/noise/BN_128x128_R.png");
            m_defaultTextures.blueNoise_128x128_R = createTexture2D("BlueNoise_128x128_R", blueNoiseImage_128x128, sampler);

            auto blueNoiseImage_1024x1024 = AssetImporter::importImageSync("BlueNoise_1024x1024_RGBA", ASSET_PATH "textures/noise/BN_1024x1024_RGBA.png");
            m_defaultTextures.blueNoise_1024x1024_RGBA = createTexture2D("BlueNoise_1024x1024_RGBA", blueNoiseImage_1024x1024, sampler);
        }

#undef DEFAULT_TEXTURE_FOLDER

        /**
            initialize the default material 
        */ 
        createMaterial("DefaultMaterial");

        stbi_set_flip_vertically_on_load(1);
    }

    void AssetManager::import(Scene* scene, const char* filename)
    {
        // parse file extension
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf")
        {
        }
        else if (extension == ".glb")
        {
            importGlb(scene, filename);
        }
        else if (extension == ".obj")
        {
        }
    }

    void AssetManager::deferredInitAsset(Asset* asset, const AssetInitFunc& inFunc)
    {
        std::lock_guard<std::mutex> lock(singleton->deferredInitMutex);
        singleton->m_deferredInitQueue.push({ asset, inFunc });
    }

    void AssetManager::update()
    {
        const u32 workload = 4;
        for (i32 i = 0; i < workload; ++i)
        {
            std::unique_lock<std::mutex> lock(deferredInitMutex);
            if (!m_deferredInitQueue.empty())
            {
                auto task = m_deferredInitQueue.front();
                m_deferredInitQueue.pop();
                lock.unlock();

                // execute
                task.initFunc(task.asset);
            }
        }

        auto renderer = Renderer::get();
        renderer->addUIRenderCommand([this]() {
            // render an asset monitor 
            static bool pOpen = true;
            ImGui::Begin("Asset Manager", &pOpen, ImGuiWindowFlags_MenuBar);
            {
                if (ImGui::BeginMenuBar())
                {
                    if (ImGui::BeginMenu("File"))
                    {
                        if (ImGui::MenuItem("Close", "Ctrl+W")) { pOpen = false; }
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenuBar();
                }

                if (ImGui::BeginTabBar("Asset Tabs"))
                {
                    if (ImGui::BeginTabItem("Mesh", nullptr))
                    {
                        ImGui::Text("Num of Meshes: %d", m_meshes.size());
                        ImGui::Separator();

                        // Left
                        static int selected = 0;
                        {
                            ImGui::BeginChild("Mesh List", ImVec2(150, 0), true);
                            for (int i = 0; i < m_meshes.size(); i++)
                            {
                                // FIXME: Good candidate to use ImGuiSelectableFlags_SelectOnNav
                                char label[128];
                                sprintf(label, "%s", m_meshes[i]->name.c_str());
                                if (ImGui::Selectable(label, selected == i))
                                    selected = i;
                            }
                            ImGui::EndChild();
                        }
                        ImGui::SameLine();

                        auto mesh = m_meshes[selected];
                        // Right
                        ImGui::BeginGroup();
                        ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us
                        if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
                        {
                            if (ImGui::BeginTabItem("Details"))
                            {
                                ImGui::Text("Mesh: %s", mesh->name.c_str());
                                ImGui::Text("Num of Submeshes: %d", mesh->numSubmeshes());
                                ImGui::Text("Num of Instances: %d", mesh->numInstances());
                                ImGui::Text("Num of Vertices: %d", mesh->numVertices());
                                ImGui::Text("Num of Indices: %d", mesh->numIndices());
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                        ImGui::EndChild();
                        ImGui::EndGroup();

                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Image"))
                    {
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Texture"))
                    {
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
        });
    }

    StaticMesh* AssetManager::createStaticMesh(const char* name)
    {
        StaticMesh* outMesh = nullptr;
        auto entry = singleton->m_meshMap.find(name);
        if (entry == singleton->m_meshMap.end())
        {
            outMesh = new StaticMesh(name);
            singleton->m_meshMap.insert({ name, outMesh });
            singleton->m_meshes.push_back(outMesh);
        }
        else
        {
            outMesh = entry->second;
        }
        return outMesh;
    }

    StaticMesh* AssetManager::createStaticMesh(const char* name, u32 numSubmeshes)
    {
        StaticMesh* outMesh = nullptr;
        auto entry = singleton->m_meshMap.find(name);
        if (entry == singleton->m_meshMap.end())
        {
            outMesh = new StaticMesh(name, numSubmeshes);
            singleton->m_meshMap.insert({ name, outMesh });
            singleton->m_meshes.push_back(outMesh);
        }
        else
        {
            outMesh = entry->second;
        }
        return outMesh;
    }

    // treat all the meshes inside one obj file as submeshes
    StaticMesh* AssetManager::importWavefrontObj(const char* meshName, const char* baseDir, const char* filename) 
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

        StaticMesh* outMesh = createStaticMesh(meshName);

        for (u32 s = 0; s < shapes.size(); ++s)
        {
            // load triangle mesh
            if (shapes[s].mesh.indices.size() > 0)
            {
                std::vector<Triangles::Vertex> vertices;
                std::vector<u32> indices(shapes[s].mesh.indices.size());

                std::unordered_map<Triangles::Vertex, u32> uniqueVerticesMap;
                u32 numUniqueVertices = 0;

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

                outMesh->addSubmeshDeferred(new Triangles(vertices, indices));
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

                outMesh->addSubmeshDeferred(new Lines(vertices, indices));
            }
        }

        return outMesh;
    }

    Image* AssetManager::createImage(const char* name)
    {
        assert(name);

        Image* outImage = nullptr;

        auto entry = singleton->m_imageMap.find(name);
        if (entry == singleton->m_imageMap.end())
        {
            outImage = new Image(name);
            singleton->m_images.push_back(outImage);
            singleton->m_imageMap.insert({ outImage->name, outImage});
        }
        outImage = singleton->m_imageMap[std::string(name)];
        return outImage;
    }

    GfxTexture2D* AssetManager::importGltfTexture(tinygltf::Model& model, tinygltf::Texture& gltfTexture)
    {
        return nullptr;
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
            StaticMesh* mesh = getAsset<StaticMesh>(meshName);
            auto staticMeshEntity = scene->createStaticMeshEntity(entityName, localTransform, mesh, parent);
            entity = staticMeshEntity;

            // setup material
            auto& gltfMesh = model.meshes[node.mesh];
            for (u32 sm = 0u; sm < gltfMesh.primitives.size(); ++sm)
            {
                std::string matlName("DefaultMaterial");

#if 1
                auto& primitive = gltfMesh.primitives[sm];
                if (primitive.material > -1)
                {
                    auto getTexture = [&](i32 imageIndex)
                    {
                        Cyan::Texture2D* texture = nullptr;
                        if (imageIndex > -1 && imageIndex < model.images.size())
                        {
                            auto& image = model.images[imageIndex];
                            texture = getAsset<Texture2D>(image.name.c_str());
                            if (!texture)
                            {
                                texture = getAsset<Texture2D>(image.uri.c_str());
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
                    Material* matl = createMaterial(matlName.c_str());
                    matl->albedoMap = getTexture(pbr.baseColorTexture.index);
                    matl->normalMap = getTexture(gltfMaterial.normalTexture.index);
                    matl->metallicRoughnessMap = getTexture(pbr.metallicRoughnessTexture.index);
                    matl->occlusionMap = getTexture(gltfMaterial.occlusionTexture.index);
                    matl->albedo = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
                    matl->roughness = pbr.roughnessFactor;
                    matl->metallic = pbr.metallicFactor;
                    glm::vec3 emissiveColor(gltfMaterial.emissiveFactor[0], gltfMaterial.emissiveFactor[1], gltfMaterial.emissiveFactor[2]);
                    f32 emissiveIntensity = glm::length(emissiveColor);
                    if (emissiveIntensity > 0.f) 
                    {
                        matl->albedo = glm::vec4(glm::normalize(emissiveColor), 1.f);
                        matl->emissive = emissiveIntensity;
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
    }

    // todo: implement this!!!
    /** note - @min:
    * first load and parse the json part, 
    * only load in geometry data first, worry about materials later
    */
    void AssetManager::importGlb(Scene* scene, const char* filename)
    {
        ScopedTimer timer("Custom import .glb timer", true);
        gltf::Glb glb(filename);
        // glb.importScene(scene);
    }

    Cyan::StaticMesh* AssetManager::importGltfMesh(tinygltf::Model& model, tinygltf::Mesh& gltfMesh) 
    {
        assert(gltfMesh.name.c_str());
        StaticMesh* outMesh = createStaticMesh(gltfMesh.name.c_str());

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

                outMesh->addSubmeshDeferred(new Triangles(vertices, indices));
            } break;
            case TINYGLTF_MODE_LINE:
            case TINYGLTF_MODE_POINTS:
            case TINYGLTF_MODE_LINE_STRIP:
            case TINYGLTF_MODE_TRIANGLE_STRIP:
            default:
                break;
            }
        } // primitive (submesh)

        return outMesh;
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
            tinygltf::Scene& gltfScene = model.scenes[model.defaultScene];
            // import textures
            {
                ScopedTimer textureImportTimer("gltf texture import timer", true);
                singleton->importGltfTextures(model);
            }
            // import meshes
            {
                ScopedTimer meshImportTimer("gltf mesh import timer", true);
                for (auto& gltfMesh : model.meshes)
                {
                    singleton->importGltfMesh(model, gltfMesh);
                }
            }
            // import gltf nodes ...?
            for (u32 i = 0; i < gltfScene.nodes.size(); ++i)
            {
                ScopedTimer entityImportTimer("gltf scene hierarchy import timer", true);
                singleton->importGltfNode(scene, model, scene->m_rootEntity, model.nodes[gltfScene.nodes[i]]);
            }
        }
        else if (extension == ".glb")
        {
            importGlb(scene, filename);
        }
    }

    Texture2D* AssetManager::createTexture2D(const char* name, Image* srcImage, const Sampler2D& inSampler)
    {
        Texture2D* outTexture = getAsset<Texture2D>(name);
        if (!outTexture)
        {
            outTexture = new Texture2D(name, srcImage, inSampler);
            singleton->addTexture(outTexture);
        }
        return outTexture;
    }

    Material* AssetManager::createMaterial(const char* name) 
    {
        Material* outMaterial = nullptr;
        std::string key = std::string(name);
        i32 materialIndex = -1;
        auto entry = singleton->m_materialMap.find(key);
        if (entry == singleton->m_materialMap.end()) 
        {
            outMaterial = new Material(name);
            materialIndex = singleton->m_materials.size();
            singleton->m_materialMap.insert({ key, materialIndex });
            singleton->m_materials.push_back(outMaterial);
        }
        else
        {
            outMaterial = singleton->m_materials[entry->second];
        }
        return outMaterial;
    }

    i32 AssetManager::getMaterialIndex(Material* material)
    {
        i32 outIndex = -1;
        auto entry = singleton->m_materialMap.find(material->name);
        if (entry != singleton->m_materialMap.end())
        {
            outIndex = entry->second;
        }
        return outIndex;
    }
}