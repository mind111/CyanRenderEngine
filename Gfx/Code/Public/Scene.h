#pragma once

#include "GfxStaticMesh.h"
#include "Lights.h"

namespace Cyan
{
    class StaticMesh;
    class StaticMeshInstance;
    class GfxMaterialInstance;

    struct StaticSubMeshInstance
    {
        std::string key;
        GfxStaticSubMesh* subMesh = nullptr;
        GfxMaterialInstance* material = nullptr;
        glm::mat4 localToWorldMatrix = glm::mat4(1.f);
    };

    // todo: design & implement this
    class GFX_API Scene
    {
    public:
        friend class Engine;
        friend class SceneRenderer;

        Scene();
        ~Scene();

        // static mesh
        StaticSubMeshInstance& findStaticSubMeshInstance(const std::string& instanceKey, bool& bFound);
        void addStaticSubMeshInstance(const StaticSubMeshInstance& instance);
        void removeStaticSubMeshInstance();
        // directional light
        void addDirectionalLight(DirectionalLight* directionalLight);
        void removeDirectionalLight(DirectionalLight* directionalLight);

    private:
        std::queue<u32> m_freeStaticSubMeshInstanceSlots;
        std::unordered_map<std::string, u32> m_staticSubMeshInstanceMap;
        std::vector<StaticSubMeshInstance> m_staticSubMeshInstances;

        DirectionalLight* m_directionalLight = nullptr;
    };
}