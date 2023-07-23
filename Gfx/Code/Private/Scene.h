#pragma once

#include "GfxInterface.h"

namespace Cyan
{
    class StaticMesh;
    class StaticMeshInstance;
    class GfxStaticSubMesh;

    // todo: design & implement this
    class Scene : public IScene
    {
    public:
        friend class SceneRenderer;

        Scene();
        virtual ~Scene() override;

        virtual void addStaticSubMeshInstance(const StaticSubMeshInstance& staticSubMeshInstance) override;

        // todo: implement this
        void removeStaticMeshInstance(StaticMeshInstance* staticMeshInstance);
    private:
        std::vector<StaticSubMeshInstance> m_staticSubMeshInstances;
    };
}