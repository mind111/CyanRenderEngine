#pragma once

#include "GfxInterface.h"

namespace Cyan
{
    class StaticMesh;
    class StaticMeshInstance;
    class GfxStaticSubMesh;
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

        StaticSubMeshInstance& findStaticSubMeshInstance(const std::string& instanceKey, bool& bFound);
        void addStaticSubMeshInstance(const StaticSubMeshInstance& instance);
        void removeStaticSubMeshInstance();

    private:
        std::queue<u32> m_freeStaticSubMeshInstanceSlots;
        std::unordered_map<std::string, u32> m_staticSubMeshInstanceMap;
        std::vector<StaticSubMeshInstance> m_staticSubMeshInstances;
    };
}