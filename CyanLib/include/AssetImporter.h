#pragma once

#include <memory>

#include "Singleton.h"

namespace Cyan
{
    namespace gltf
    {
        struct Gltf;
        struct Glb;
        struct Mesh;
        struct Image;
        struct Node;
    }

    class AssetManager;
    class GltfImporter;
    class World;
    class Entity;
    class StaticMesh;
    class Image;

    class AssetImporter : public Singleton<AssetImporter>
    {
    public:
        AssetImporter(AssetManager* assetManager);
        ~AssetImporter();

        static void import(World* world, const char* filename);
        static void importAsync(World* world, const char* filename);
        static std::shared_ptr<StaticMesh> importWavefrontObj(const char* name, const char* filename);
        static std::shared_ptr<Cyan::Image> importImage(const char* name, const char* filename);

    private:
        AssetManager* m_assetManager = nullptr;
        std::unique_ptr<GltfImporter> m_gltfImporter = nullptr;
    };

    class GltfImporter
    {
    public:
        GltfImporter(AssetImporter* inOwner);
        ~GltfImporter() { }

        void import(World* world, const char* filename);
        void importAsync(World* world, const char* filename);

    private:
        void importSceneNodes(World* world, gltf::Gltf& gltf);
        void importSceneNode(World* world, gltf::Gltf& gltf, Entity* parent, const gltf::Node& node);

        AssetImporter* m_owner = nullptr;
    };
}
