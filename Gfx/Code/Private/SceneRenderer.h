#pragma once

#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "GfxModule.h"
#include "SceneView.h"

namespace Cyan
{
    class Scene;
    class SceneView;

    class SceneRenderer
    {
    public:
        static SceneRenderer* get();
        virtual ~SceneRenderer();

        void render(Scene* scene, SceneView& sceneView);

        void renderSceneDepth(GHDepthTexture* outDepth, Scene* scene, const SceneView::State& viewState);
        void renderSceneGBuffer(Scene* scene, SceneView& sceneView);
        void renderSceneDirectLighting(Scene* scene, SceneView& sceneView);

        void tonemapping(GHTexture2D* dstTexture, GHTexture2D* srcTexture);
    private:
        SceneRenderer();

        static SceneRenderer* s_renderer;
    };
} 