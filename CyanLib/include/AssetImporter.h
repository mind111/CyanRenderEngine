#pragma once

#include "ExternalAssetFile.h"
#include "Asset.h"
#include "Mesh.h"
#include "Image.h"
#include "Scene.h"
#include "Singleton.h"

namespace Cyan
{
    namespace gltf
    {
        struct Gltf;
        struct Glb;
        struct Mesh;
        struct Node;
    }

    class GltfImporter;
    struct Entity;

    class AssetImporter : public Singleton<AssetImporter>
    {
    public:
        AssetImporter();
        ~AssetImporter() { }

        static void import(Scene* scene, const char* filename);
        static void importAsync(Scene* scene, const char* filename);
        static void import(Asset* outAsset);

        void registerAssetSrcFile(Asset* asset, const char* filename);
    private:
        std::unique_ptr<GltfImporter> m_gltfImporter = nullptr;
        std::unordered_map<std::string, const char*> m_assetToSrcMap;
    };

    class GltfImporter
    {
    public:
        struct GltfAssetMapping
        {
            std::shared_ptr<gltf::Gltf> src = nullptr;

            std::unordered_map<std::string, i32> meshMap;
            std::unordered_map<std::string, i32> imageMap;
            std::unordered_map<std::string, i32> textureMap;
            std::unordered_map<std::string, i32> materialMap;
        };

        GltfImporter(AssetImporter* inOwner);
        ~GltfImporter() { }

        void import(Scene* scene, const char* filename);
        void importAsync(Scene* scene, const char* filename);
        void import(Asset* outAsset);

    private:
        void importMesh(StaticMesh* outMesh, gltf::Gltf& srcGltf, const gltf::Mesh& srcGltfMesh);
        void importSceneAsync(Scene* outScene, gltf::Gltf& gltf);
        void importSceneNodeAsync(Scene* outScene, gltf::Gltf& gltf, Entity* parent, const gltf::Node& node);

        AssetImporter* m_owner = nullptr;

        // maps an asset's name to a gltf asset mapping
        std::unordered_map<std::string, std::shared_ptr<GltfAssetMapping>> m_assetToGltfMap;
        // todo: maps a filename to a gltf asset mapping

        std::vector<std::shared_ptr<GltfAssetMapping>> m_gltfAssetMappings;
    };
}
