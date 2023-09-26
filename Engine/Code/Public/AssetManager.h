#pragma once

#include "Core.h"
#include "Engine.h"

namespace Cyan
{
    namespace gltf
    {
        class Gltf;
        struct Node;
    }

    class World;
    class Entity;
    class Asset;
    class StaticMesh;
    class Image;
    class Texture2D;
    class GHTexture2D;
    class GHSampler2D;
    class Material;
    class MaterialInstance;

    using MaterialSetupFunc = std::function<void(Material*)>;

    class ENGINE_API AssetManager
    {
    public:
        ~AssetManager();

        static AssetManager* get();
        void initialize();
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
        static Image* createImage(const char* name);
        static Texture2D* createTexture2D(const char* name, const GHSampler2D& sampler2D, Image* image, bool bGenerateMipmap);
        static Texture2D* createTexture2D(const char* name, std::unique_ptr<GHTexture2D> GHTexture);
        static Texture2D* createTexture2DFromImage(const char* imageFilePath, const char* textureName, const GHSampler2D& sampler2D);
        static Material* createMaterial(const char* name, const char* materialSourcePath, const MaterialSetupFunc& setupFunc);
        static MaterialInstance* createMaterialInstance(const char* name, Material* parent);

    private:
        AssetManager();

        void importGltf(const char* gltfFilename);
        void importGltf(World* world, const char* gltfFilename);

        std::unordered_map<std::string, Asset*> m_residentAssetMap;
        static std::unique_ptr<AssetManager> s_instance;
    };
}
