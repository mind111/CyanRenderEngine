#pragma once

#include "GfxInterface.h"

namespace Cyan
{
    class StaticMesh;
    class StaticMeshInstance;
    class GfxStaticSubMesh;

    // todo: design & implement this
    class GFX_API Scene
    {
    public:
        friend class Engine;
        friend class SceneRenderer;

        Scene();
        ~Scene();

    private:
        std::vector<StaticSubMeshInstance> m_staticSubMeshInstances;
    };
}