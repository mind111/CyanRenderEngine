#include "gltf.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "Entity.h"

namespace Cyan
{
    AssetImporter* Singleton<AssetImporter>::singleton = nullptr;
    AssetImporter::AssetImporter()
    {
        m_gltfImporter = std::make_unique<GltfImporter>(this);
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

            struct PendingMeshImportTask
            {
                StaticMesh* mesh = nullptr;
                gltf::Mesh* gltfMesh = nullptr;
            };
            std::vector<PendingMeshImportTask> pendingMeshImports;

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

                pendingMeshImports.push_back({ mesh, &glb->meshes[i] });
            }

            // only use one background thread to handle async loading for now
            auto asyncImportExec = [this, pendingMeshImports, glb]() {
                for (const auto& task : pendingMeshImports)
                {
                    importMesh(task.mesh, *glb, *task.gltfMesh);
                }
            };

            std::thread asyncImportThread(asyncImportExec);
            asyncImportThread.detach();

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
                    importMesh(outMesh, *src, gltfMesh);
                    outMesh->onLoaded();
                }
            }
            else if (outAsset->getAssetTypeName() == "Image")
            {
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
        assert(outMesh->numSubmeshes() == 0);

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
}