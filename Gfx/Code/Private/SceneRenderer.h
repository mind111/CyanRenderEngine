#pragma once

#include "GfxInterface.h"
#include "GHTexture.h"

namespace Cyan
{
    class Scene;
    class ISceneRender;

    class SceneRenderer : public ISceneRenderer
    {
    public:
        SceneRenderer();
        virtual ~SceneRenderer();

        virtual void render(ISceneRender* outRender, IScene* scene, const SceneViewState& viewInfo) override;

        void renderSceneDepth(GHDepthTexture* outDepth, Scene* scene, const SceneViewState& viewInfo);
    private:
    };
} 