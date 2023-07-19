#pragma once

#include "Core.h"

namespace Cyan
{
    class World;
    class StaticMesh;
    // class Material;
    // class Texture2D;

    class AssetManager
    {
    public:
        ~AssetManager();

        static AssetManager* get();
        static void import(World* world, const char* filename);
        static StaticMesh* createStaticMesh(const char* name, u32 numSubMeshes);
        // static Texture2D* createTexture2D(const char* name, u32 numSubMeshes);
        // static Material* createTexture2D(const char* name, u32 numSubMeshes);

    private:
        AssetManager();

        void importGltf(const char* gltfFilename);

        std::unordered_map<std::string, StaticMesh*> m_staticMeshMap;
        static std::unique_ptr<AssetManager> s_instance;
    };
}
