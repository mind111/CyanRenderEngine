#pragma once

#include "GfxInterface.h"
#include "GHTexture.h"
#include "GfxModule.h"

namespace Cyan
{
    class Scene;
    class SceneView;

    class SceneRenderer
    {
    public:
        SceneRenderer();
        virtual ~SceneRenderer();

        void render(Scene* scene, SceneView& sceneView);

        void renderSceneDepth(GHDepthTexture* outDepth, Scene* scene, const SceneView::State& viewState);
    private:
    };
} 