#pragma once

#include "Core.h"
#include "Engine.h"

namespace Cyan
{
    namespace gltf
    {
        class Gltf;
        class Node;
    }

    class World;
    class Entity;
    class Asset;
    class StaticMesh;

    // class Material;
    // class Texture2D;
    class ENGINE_API AssetManager
    {
    public:
        ~AssetManager();

        static AssetManager* get();
        static void import(World* world, const char* filename);

        template <typename T>
        static T* findAsset(const std::string& name)
        {
            T* outAsset = nullptr;
            auto entry = s_instance->m_residentAssetMap.find(name);
            if (entry != s_instance->m_residentAssetMap.end())
            {
                outAsset = dynamic_cast<T*>(entry->second);
            }
            if (outAsset == nullptr)
            {
                // todo: do something here ...
            }
            return outAsset;
        }

        static StaticMesh* createStaticMesh(const char* name, u32 numSubMeshes);
        // static Texture2D* createTexture2D(const char* name, u32 numSubMeshes);
        // static Material* createTexture2D(const char* name, u32 numSubMeshes);

    private:
        AssetManager();

        void importGltf(World* world, const char* gltfFilename);

        std::unordered_map<std::string, Asset*> m_residentAssetMap;
        static std::unique_ptr<AssetManager> s_instance;
    };
}
