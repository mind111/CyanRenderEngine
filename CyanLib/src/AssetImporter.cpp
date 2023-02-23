#include <condition_variable>
#include <algorithm>

#include "gltf.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "Entity.h"

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
            auto numThreads = std::max(std::thread::hardware_concurrency(), 1u);
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
    AssetImporter::AssetImporter()
    {
        m_gltfImporter = std::make_unique<GltfImporter>(this);
        s_taskManager = std::make_unique<TaskManager>();
        s_taskManager->init();
    }

    void AssetImporter::import(Scene* scene, const char* filename)
    {
        // parse file extension
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf" || extension == ".glb")
        {
            singleton->m_gltfImporter->import(scene, filename);
        }
        else if (extension == ".obj")
        {
        }
    }

    void AssetImporter::importAsync(Scene* scene, const char* filename)
    {
        // parse file extension
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf" || extension == ".glb")
        {
            singleton->m_gltfImporter->importAsync(scene, filename);
        }
        else if (extension == ".obj")
        {
        }
    }

    void AssetImporter::import(Asset* outAsset) 
    { 
        auto entry = singleton->m_assetToSrcMap.find(outAsset->name);
        if (entry != singleton->m_assetToSrcMap.end())
        {
            std::string path(entry->second);
            u32 found = path.find_last_of('.');
            std::string extension = path.substr(found, found + 1);
            if (extension == ".gltf" || extension == ".glb")
            {
                singleton->m_gltfImporter->import(outAsset);
            }
        }
    }

    Image* AssetImporter::importImageSync(const char* imageName, const char* filename)
    {
        Image* outImage = AssetManager::getAsset<Image>(imageName);
        if (outImage == nullptr)
        {
            outImage = AssetManager::createImage(imageName);
            singleton->registerAssetSrcFile(outImage, filename);
            // import image data immediately
            singleton->importImageInternal(outImage, filename);
        }
        return outImage;
    }

    Image* AssetImporter::importImageAsync(const char* imageName, const char* filename)
    {
        Image* outImage = AssetManager::getAsset<Image>(imageName);
        if (outImage == nullptr)
        {
            outImage = AssetManager::createImage(imageName);
            singleton->registerAssetSrcFile(outImage, filename);

            AsyncTask task([outImage, filename]() {
                singleton->importImageInternal(outImage, filename);
            });

            // async loading image data
            s_taskManager->enqueueTask(task);
        }
        return outImage;
    }

    void AssetImporter::importImageInternal(Image* outImage, const char* filename)
    {
        i32 hdr = stbi_is_hdr(filename);
        if (hdr)
        {
            outImage->bitsPerChannel = 32;
            outImage->pixels = std::shared_ptr<u8>((u8*)stbi_loadf(filename, &outImage->width, &outImage->height, &outImage->numChannels, 0));
        }
        else
        {
            i32 is16Bit = stbi_is_16_bit(filename);
            if (is16Bit)
            {
                outImage->pixels = std::shared_ptr<u8>((u8*)stbi_load_16(filename, &outImage->width, &outImage->height, &outImage->numChannels, 0));
                outImage->bitsPerChannel = 16;
            }
            else
            {
                outImage->pixels = std::shared_ptr<u8>(stbi_load(filename, &outImage->width, &outImage->height, &outImage->numChannels, 0));
                outImage->bitsPerChannel = 8;
            }
        }
        assert(outImage->pixels);
    }

    void AssetImporter::registerAssetSrcFile(Asset* asset, const char* filename)
    {
        m_assetToSrcMap.insert({ asset->name, filename });
    }

    GltfImporter::GltfImporter(AssetImporter* inOwner)
        : m_owner(inOwner)
    {

    }

    void GltfImporter::import(Scene* scene, const char* filename)
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
            auto glb = std::make_shared<gltf::Glb>(filename);
            glb->load();

            m_gltfAssetMappings.push_back(std::make_shared<GltfAssetMapping>());
            auto mapping = m_gltfAssetMappings.back();
            mapping->src = glb;

            // import meshes
            for (i32 i = 0; i < glb->meshes.size(); ++i)
            {
                const auto& gltfMesh = glb->meshes[i];
                std::string meshName = gltfMesh.name;
                if (meshName.empty())
                {

                }
                // It's better for AssetImporter to manage the mapping between a external gltfMesh and StaticMesh because
                // this way, Glb doesn't need to be aware of StaticMesh, keep them decoupled. It's the AssetImporter's
                auto mesh = AssetManager::createStaticMesh(meshName.c_str());
                mapping->meshMap.insert({ meshName, i });
                m_assetToGltfMap.insert({ meshName, mapping });
                m_owner->registerAssetSrcFile(mesh, filename);
                importMesh(mesh, *glb, gltfMesh);
            }

            // import images 

            // import textures

            // import materials

            // import scene hierarchy
        }
    }

    void GltfImporter::importAsync(Scene* scene, const char* filename)
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
            auto glb = std::make_shared<gltf::Glb>(filename);
            glb->load();

            m_gltfAssetMappings.push_back(std::make_shared<GltfAssetMapping>());
            auto mapping = m_gltfAssetMappings.back();
            mapping->src = glb;

            // lazy import meshes
            for (i32 i = 0; i < glb->meshes.size(); ++i)
            {
                const auto& gltfMesh = glb->meshes[i];
                std::string meshName = gltfMesh.name;
                if (meshName.empty())
                {
                    std::string prefix(filename);
                    meshName = prefix + '/' + "mesh_" + std::to_string(i);
                }
                // It's better for AssetImporter to manage the mapping between a external gltfMesh and StaticMesh because
                // this way, Glb doesn't need to be aware of StaticMesh, keep them decoupled. It's the AssetImporter's
                auto mesh = AssetManager::createStaticMesh(meshName.c_str());
                mapping->meshMap.insert({ meshName, i });
                m_assetToGltfMap.insert({ meshName, mapping });
                m_owner->registerAssetSrcFile(mesh, filename);
            }

#if 0
            // lazy import images
            for (i32 i = 0; i < glb->images.size(); ++i)
            {
                const auto& gltfImage = glb->images[i];
                std::string imageName = gltfImage.name;
                if (imageName.empty())
                {

                }
                auto image = AssetManager::createImage(imageName.c_str());
            }

            // import textures
            for (i32 i = 0; i < glb->textures.size(); ++i)
            {
                const gltf::Texture& texture = glb->textures[i];
                const gltf::Image& gltfImage = glb->images[texture.source];
                const gltf::Sampler& gltfSampler = glb->samplers[texture.sampler];

                Cyan::Image* image = AssetManager::getAsset<Cyan::Image>(gltfImage.name.c_str());

                Sampler2D sampler;
                bool bGenerateMipmap = false;
                gltf::translateSampler(gltfSampler, sampler, bGenerateMipmap);
                AssetManager::createTexture2DBindless(texture.name.c_str(), image, sampler);
            }
#endif

            // import materials
#if 0
            // only use one background thread to handle async loading for now
            auto asyncImportExec = [this, pendingMeshImports, glb]() {
                for (const auto& task : pendingMeshImports)
                {
                    importMesh(task.mesh, *glb, *task.gltfMesh);
                }
                // todo: async loading image, texture and materials
            };

            std::thread asyncImportThread(asyncImportExec);
            asyncImportThread.detach();
#endif

            // import scene hierarchy
            importSceneAsync(scene, *glb);
        }
    }

    void GltfImporter::importSceneAsync(Scene* outScene, gltf::Gltf& gltf)
    {
        // load scene hierarchy 
        if (gltf.defaultScene >= 0)
        {
            const gltf::Scene& gltfScene = gltf.scenes[gltf.defaultScene];
            for (i32 i = 0; i < gltfScene.nodes.size(); ++i)
            {
                const gltf::Node& node = gltf.nodes[gltfScene.nodes[i]];
                importSceneNodeAsync(outScene, gltf, nullptr, node);
            }
        }
    }
    
    void GltfImporter::importSceneNodeAsync(Scene* outScene, gltf::Gltf& gltf, Entity* parent, const gltf::Node& node)
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
           const gltf::Mesh& gltfMesh = gltf.meshes[node.mesh];
           Cyan::StaticMesh* mesh = AssetManager::getAsset<Cyan::StaticMesh>(gltfMesh.name.c_str());
           if (mesh->state == Asset::State::kUnloaded)
           {
               mesh->import();
           }

           StaticMeshEntity* staticMeshEntity = outScene->createStaticMeshEntity(name.c_str(), t, mesh, parent);
           staticMeshEntity->setMaterial(AssetManager::getAsset<Cyan::Material>("DefaultMaterial"));
           e = staticMeshEntity;
        }
        else
        {
           e = outScene->createEntity(name.c_str(), t, parent);
        }

        // recurse into children nodes
        for (auto child : node.children)
        {
            importSceneNodeAsync(outScene, gltf, e, gltf.nodes[child]);
        }
    }
 
    // this function will likely need to be invoked from worker threads
    void GltfImporter::import(Asset* outAsset)
    {
        auto gltfFileEntry = m_assetToGltfMap.find(outAsset->name);
        if (gltfFileEntry != m_assetToGltfMap.end())
        {
            auto mapping = gltfFileEntry->second;
            auto src = mapping->src;
            if (outAsset->getAssetTypeName() == "StaticMesh")
            {
                StaticMesh* outMesh = dynamic_cast<StaticMesh*>(outAsset);
                auto entry = mapping->meshMap.find(outAsset->name);
                if (entry != mapping->meshMap.end())
                {
                    i32 meshIndex = entry->second;
                    const gltf::Mesh& gltfMesh = mapping->src->meshes[meshIndex];

                    AsyncTask task([this, outMesh, src, &gltfMesh]() {
                        importMesh(outMesh, *src, gltfMesh);
                    });

                    s_taskManager->enqueueTask(task);
                }
            }
            else if (outAsset->getAssetTypeName() == "Image")
            {
                Image* outImage = dynamic_cast<Image*>(outAsset);
                auto entry = mapping->imageMap.find(outAsset->name);
                if (entry != mapping->imageMap.end())
                {
                    i32 imageIndex = entry->second;
                    const gltf::Image& gltfImage = mapping->src->images[imageIndex];

                    AsyncTask task([this, outImage, src, &gltfImage]() {
                        importImage(outImage, *src, gltfImage);
                        outImage->onLoaded();
                    });

                    s_taskManager->enqueueTask(task);
                }
            }
            else if (outAsset->getAssetTypeName() == "Texture2D")
            {

            }
        }
        else
        {
            assert(0);
        }
    }

    void GltfImporter::importMesh(StaticMesh* outMesh, gltf::Gltf& srcGltf, const gltf::Mesh& srcGltfMesh)
    {
        // make sure that we are not re-importing same mesh multiple times
        // assert(outMesh->numSubmeshes() == 0);

        i32 numSubmeshes = srcGltfMesh.primitives.size();
        for (i32 sm = 0; sm < numSubmeshes; ++sm)
        {
            const gltf::Primitive& p = srcGltfMesh.primitives[sm];

            Geometry* geometry = nullptr;
            switch ((gltf::Primitive::Mode)p.mode)
            {
            case gltf::Primitive::Mode::kTriangles: {
                // todo: this also needs to be tracked and managed by the AssetManager using some kind of GUID system
                geometry = new Triangles();
                Triangles* triangles = static_cast<Triangles*>(geometry);
                srcGltf.importTriangles(p, *triangles);
                outMesh->addSubmeshDeferred(triangles);
            } break;
            case gltf::Primitive::Mode::kLines:
            case gltf::Primitive::Mode::kPoints:
            default:
                assert(0);
            }
        }
    }

    void GltfImporter::importImage(Image* outImage, gltf::Gltf& gltf, const gltf::Image& gltfImage)
    {

    }
}