#pragma once

#include "GfxInterface.h"

namespace Cyan
{
    class StaticMesh;
    class StaticMeshInstance;
    class GfxStaticSubMesh;

    struct StaticSubMeshInstance
    {
        // todo: material
        GfxStaticSubMesh* subMesh = nullptr;
        glm::mat4 localToWorldMatrix = glm::mat4(1.f);
    };

    // todo: design & implement this
    class Scene : public IScene
    {
    public:
        friend class SceneRenderer;

        Scene();
        virtual ~Scene() override;

        virtual void addStaticMeshInstance(StaticMeshInstance* staticMeshInstance) override;

        // todo: implement this
        void removeStaticMeshInstance(StaticMeshInstance* staticMeshInstance);
    private:
        std::vector<StaticSubMeshInstance> m_staticSubMeshInstances;
    };
}