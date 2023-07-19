#pragma once

#include "GfxInterface.h"

namespace Cyan
{
    class StaticMesh;
    class StaticMeshInstance;

    // todo: design & implement this
    class Scene : public IScene
    {
    public:
        friend class SceneRenderer;

        Scene();
        virtual ~Scene() override;

        void addStaticMeshInstance(StaticMeshInstance* instance);
        void removeStaticMeshInstance(StaticMeshInstance* instance);
    private:
        std::vector<StaticMeshInstance*> m_staticMeshInstances;
    };
}