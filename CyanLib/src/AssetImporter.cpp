#include <thread>
#include <algorithm>

#include <tiny_obj/tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include "gltf.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "World.h"
#include "Entity.h"
#include "Image.h"
#include "StaticMeshEntity.h"

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
    struct AsyncTask
    {
        // todo: learn about move semantic and whether this does save a copy or not
        AsyncTask(std::function<void()>&& inFunc)
            : loadFunc(std::move(inFunc))
        {

        }

        ~AsyncTask() { }

        void execute()
        {
            loadFunc();
        }

        std::function<void()> loadFunc = [](){ };
    };

    struct TaskManager
    {
        TaskManager() { }
        ~TaskManager() { }

        void init()
        { 
            i32 numThreads = std::max((i32)std::thread::hardware_concurrency(), 1);
            for (i32 i = 0; i < numThreads; ++i)
            {
                std::thread worker([this]() {
                    runWorker();
                });
                worker.detach();
                threadPool.push_back(std::move(worker));
            }
        }

        void enqueueTask(const AsyncTask& task)
        {
            std::lock_guard<std::mutex> lock(taskQueueMutex);
            taskQueue.push(task);
            wakeCondition.notify_one();
        }

        void runWorker()
        {
            while (true)
            {
                std::unique_lock<std::mutex> lock(taskQueueMutex);
                wakeCondition.wait(lock, [this]() { return !taskQueue.empty(); });
                AsyncTask task = taskQueue.front();
                taskQueue.pop();
                lock.unlock();
                task.execute();
            }
        }

        std::mutex taskQueueMutex;
        std::queue<AsyncTask> taskQueue;
        std::condition_variable wakeCondition;
        std::vector<std::thread> threadPool;
    };

    static std::unique_ptr<TaskManager> s_taskManager = nullptr;

    AssetImporter* Singleton<AssetImporter>::singleton = nullptr;
    AssetImporter::AssetImporter(AssetManager* assetManager)
        : m_assetManager(assetManager)
    {
        m_gltfImporter = std::make_unique<GltfImporter>(this);
        s_taskManager = std::make_unique<TaskManager>();
        s_taskManager->init();
    }

    AssetImporter::~AssetImporter()
    {

    }

    void AssetImporter::import(World* world, const char* filename)
    {
        // parse file extension
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf" || extension == ".glb")
        {
            singleton->m_gltfImporter->importAsync(world, filename);
        }
        else if (extension == ".obj")
        {
        }
    }

    void AssetImporter::importAsync(World* world, const char* filename)
    {
        // parse file extension
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf" || extension == ".glb")
        {
            singleton->m_gltfImporter->importAsync(world, filename);
        }
        else if (extension == ".obj")
        {
        }
    }

    static bool operator==(const Triangles::Vertex& lhs, const Triangles::Vertex& rhs) {
        // todo: maybe use memcmp() here instead ..?
        // bit equivalence
        return (lhs.pos == rhs.pos)
            && (lhs.normal == rhs.normal)
            && (lhs.tangent == rhs.tangent)
            && (lhs.texCoord0 == rhs.texCoord0);
    }

    // treat all the meshes inside one obj file as submeshes
    std::shared_ptr<StaticMesh> AssetImporter::importWavefrontObj(const char* name, const char* filename) 
    {
        auto calculateTangent = [](std::vector<Triangles::Vertex>& vertices, u32 face[3]) {
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
        };

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> m_materials;
        std::string warn;
        std::string err;

        std::string path(filename);
        auto pos = path.find_last_of('/');
        std::string baseDir = path.substr(0, pos);

        bool ret = tinyobj::LoadObj(&attrib, &shapes, &m_materials, &warn, &err, filename, baseDir.c_str());
        if (!ret)
        {
            cyanError("Failed loading obj file %s ", filename);
            cyanError("Warnings: %s               ", warn.c_str());
            cyanError("Errors:   %s               ", err.c_str());
        }

        auto outMesh = AssetManager::createStaticMesh(name, shapes.size());

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

                auto t = std::make_unique<Triangles>(vertices, indices);
                outMesh->createSubmesh(s, std::move(t));
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

                auto l = std::make_unique<Lines>(vertices, indices);
                outMesh->createSubmesh(s, std::move(l));
            }
        }

        return outMesh;
    }


    std::shared_ptr<Image> AssetImporter::importImage(const char* name, const char* filename)
    {
        std::shared_ptr<Image> outImage = singleton->m_assetManager->createImage(name);
        i32 hdr = stbi_is_hdr(filename);
        if (hdr)
        {
            outImage->m_bitsPerChannel = 32;
            outImage->m_pixels = std::shared_ptr<u8>((u8*)stbi_loadf(filename, &outImage->m_width, &outImage->m_height, &outImage->m_numChannels, 0));
        }
        else
        {
            i32 is16Bit = stbi_is_16_bit(filename);
            if (is16Bit)
            {
                outImage->m_pixels = std::shared_ptr<u8>((u8*)stbi_load_16(filename, &outImage->m_width, &outImage->m_height, &outImage->m_numChannels, 0));
                outImage->m_bitsPerChannel = 16;
            }
            else
            {
                outImage->m_pixels = std::shared_ptr<u8>(stbi_load(filename, &outImage->m_width, &outImage->m_height, &outImage->m_numChannels, 0));
                outImage->m_bitsPerChannel = 8;
            }
        }
        assert(outImage->m_pixels != nullptr);
        outImage->onLoaded();
        return outImage;
    }

    GltfImporter::GltfImporter(AssetImporter* inOwner)
        : m_owner(inOwner)
    {

    }

    void GltfImporter::importAsync(World* world, const char* filename)
    {
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf")
        {
        }
        else if (extension == ".glb")
        {
            // glb needs to persist until all async tasks are finished
            auto glb = std::make_shared<gltf::Glb>(filename);
            glb->load();

            // async import meshes
            for (i32 m = 0; m < glb->m_meshes.size(); ++m)
            {
                const auto& gltfMesh = glb->m_meshes[m];
                std::string meshName = gltfMesh.m_name; // meshName cannot be empty here since it's handled in glb->load()
                auto mesh = AssetManager::createStaticMesh(meshName.c_str(), gltfMesh.primitives.size());

                for (i32 sm = 0; sm < gltfMesh.primitives.size(); ++sm)
                {
                    AsyncTask task([this, mesh, glb, m, sm]() {
                            const gltf::Primitive& p = glb->m_meshes[m].primitives[sm];
                            switch ((gltf::Primitive::Mode)p.mode)
                            {
                            case gltf::Primitive::Mode::kTriangles:
                            {
                                auto triangles = std::make_unique<Triangles>();
                                glb->importTriangles(p, *triangles);
                                mesh->createSubmesh(sm, std::move(triangles));
                            } break;
                            case gltf::Primitive::Mode::kLines:
                            case gltf::Primitive::Mode::kPoints:
                            default:
                                assert(0);
                            }
                    });

                    s_taskManager->enqueueTask(task);
                }
            }

            for (i32 i = 0; i < glb->m_images.size(); ++i)
            {
                const auto& gltfImage = glb->m_images[i];
                std::string imageName = gltfImage.m_name;
                auto image = AssetManager::createImage(imageName.c_str());

                AsyncTask task([this, glb, gltfImage, image]() {
                    glb->importImage(gltfImage, *image);
                    });

                s_taskManager->enqueueTask(task);
            }

            for (i32 i = 0; i < glb->m_textures.size(); ++i)
            {
                const gltf::Texture& gltfTexture = glb->m_textures[i];
                const gltf::Image& gltfImage = glb->m_images[gltfTexture.source];
                const gltf::Sampler& gltfSampler = glb->m_samplers[gltfTexture.sampler];
                std::string textureName = gltfTexture.m_name;
                auto image = AssetManager::findAsset<Image>(gltfImage.m_name.c_str());

                Sampler2D sampler;
                bool bGenerateMipmap = false;
                gltf::translateSampler(gltfSampler, sampler, bGenerateMipmap);
                auto texture = AssetManager::createTexture2D(textureName.c_str(), image, sampler);
            }

            glb->importMaterials();

            // import scene nodes
            importSceneNodes(world, *glb);
        }
    }

    void GltfImporter::importSceneNodes(World* world, gltf::Gltf& gltf)
    {
        // load scene hierarchy 
        if (gltf.m_defaultScene >= 0)
        {
            const gltf::Scene& gltfScene = gltf.m_scenes[gltf.m_defaultScene];
            for (i32 i = 0; i < gltfScene.m_nodes.size(); ++i)
            {
                const gltf::Node& node = gltf.m_nodes[gltfScene.m_nodes[i]];
                importSceneNode(world, gltf, nullptr, node);
            }
        }
    }
    
    void GltfImporter::importSceneNode(World* world, gltf::Gltf& gltf, Entity* parent, const gltf::Node& node)
    {
        // @name
        std::string m_name = node.m_name;

        // @transform
        Transform localTransform;
        if (node.hasMatrix >= 0)
        {
            const std::array<f32, 16>& m = node.matrix;
            glm::mat4 mat = {
                glm::vec4(m[0],  m[1],  m[2],  m[3]),     // column 0
                glm::vec4(m[4],  m[5],  m[6],  m[7]),     // column 1
                glm::vec4(m[8],  m[9],  m[10], m[11]),    // column 2
                glm::vec4(m[12], m[13], m[14], m[15])     // column 3
            };
            localTransform.fromMatrix(mat);
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
            localTransform.scale = scale;
            localTransform.rotation = glm::quat(rotation.w, glm::vec3(rotation.x, rotation.y, rotation.z));
            localTransform.translation = translation;
        }
        // @mesh
        Entity* e = nullptr;
        if (node.mesh >= 0)
        {
           const gltf::Mesh& gltfMesh = gltf.m_meshes[node.mesh];
           auto mesh = AssetManager::findAsset<Cyan::StaticMesh>(gltfMesh.m_name.c_str());
           StaticMeshEntity* staticMeshEntity = world->createStaticMeshEntity(m_name.c_str(), localTransform, mesh);
           e = staticMeshEntity;
           // setup materials
           for (i32 p = 0; p < gltfMesh.primitives.size(); ++p)
           {
               const gltf::Primitive& primitive = gltfMesh.primitives[p];
               if (primitive.material >= 0)
               {
                   const gltf::Material& gltfMatl = gltf.m_materials[primitive.material];
                   auto material = AssetManager::findAsset<Cyan::Material>(gltfMatl.m_name.c_str());
                   staticMeshEntity->setMaterial(material, p);
               }
           }
        }
        else
        {
           e = world->createEntity(node.m_name.c_str(), localTransform);
        }

        if (parent != nullptr)
        {
            parent->attachChild(e);
        }

        // recurse into children nodes
        for (auto child : node.children)
        {
            importSceneNode(world, gltf, e, gltf.m_nodes[child]);
        }
    }
}