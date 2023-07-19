#include "AssetManager.h"
#include "World.h"
#include "StaticMesh.h"
#include "gltf.h"

namespace Cyan
{
    AssetManager::AssetManager()
    {

    }

    AssetManager::~AssetManager()
    {

    }

    std::unique_ptr<AssetManager> AssetManager::s_instance = nullptr;
    AssetManager* AssetManager::get()
    {
        if (s_instance == nullptr)
        {
            s_instance = std::unique_ptr<AssetManager>(new AssetManager());
        }
        return s_instance.get();
    }

    void AssetManager::import(World* world, const char* filename)
    {
        // parse file extension
        std::string path(filename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf" || extension == ".glb")
        {
            s_instance->importGltf(filename);
        }
        else if (extension == ".obj")
        {
        }
    }

    void AssetManager::importGltf(const char* gltfFilename)
    {
        std::string path(gltfFilename);
        u32 found = path.find_last_of('.');
        std::string extension = path.substr(found, found + 1);
        if (extension == ".gltf")
        {
        }
        else if (extension == ".glb")
        {
            // glb needs to persist until all async tasks are finished
            auto glb = std::make_shared<gltf::Glb>(gltfFilename);
            glb->load();

            // async import meshes
            for (i32 m = 0; m < glb->m_meshes.size(); ++m)
            {
                const auto& gltfMesh = glb->m_meshes[m];
                std::string meshName = gltfMesh.m_name; // meshName cannot be empty here since it's handled in glb->load()
                u32 numSubMeshes = static_cast<u32>(gltfMesh.primitives.size());
                auto mesh = AssetManager::createStaticMesh(meshName.c_str(), numSubMeshes);

                // todo: this can be run on a worker thread, hmmm, maybe a job system ...?
                for (i32 sm = 0; sm < numSubMeshes; ++sm)
                {
                    const gltf::Primitive& p = glb->m_meshes[m].primitives[sm];
                    switch ((gltf::Primitive::Mode)p.mode)
                    {
                    case gltf::Primitive::Mode::kTriangles:
                    {
                        auto triangles = std::make_unique<Triangles>();
                        glb->importTriangles(p, *triangles);
                        auto subMesh = std::make_unique<StaticMesh::SubMesh>(std::move(triangles));
                        mesh->setSubMesh(std::move(subMesh), sm);
                    } break;
                    case gltf::Primitive::Mode::kLines:
                    case gltf::Primitive::Mode::kPoints:
                    default:
                        assert(0);
                    }
                }
            }
        }
    }

    StaticMesh* AssetManager::createStaticMesh(const char* name, u32 numSubMeshes)
    {
        return new StaticMesh(name, numSubMeshes);
    }
}