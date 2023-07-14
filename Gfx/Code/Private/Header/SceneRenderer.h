#pragma once

#include "GfxInterface.h"

namespace Cyan
{
    class ISceneRender;

    class SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        virtual ~SceneRenderer();
        virtual void render(ISceneRender* outRender, IScene* scene, const SceneViewInfo& viewInfo) override;
    };
} 