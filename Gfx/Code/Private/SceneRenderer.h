#pragma once

#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "GfxModule.h"
#include "SceneView.h"

namespace Cyan
{
    class Scene;
    class SceneView;

    struct ReSTIRSSDOSettings
    {
        bool bEnableSpatialPass = true;
        bool bApplySSAO = true;
        i32 numSpatialPasses = 1;
        i32 spatialPassSampleCount = 8;
        f32 spatialPassKernelRadius = 0.005f;
    };

    struct ReSTIRSSGISettings
    {
        bool bEnable = true;
        bool bEnableSpatialPass = false;
        i32 spatialPassSampleCount = 8;
        f32 spatialPassKernelRadius = 0.01f;
        bool bEnableDenoising = true;
        bool bNormalEdgeStopping = true;
        bool bPositionEdgeStopping = true;
        f32 sigmaN = .2f;
        f32 sigmaP = .4f;
        i32 numDenoisingPasses = 4;
        f32 indirectIrradianceBoost = 4.f;
    };

    enum class RenderMode
    {
        kResolvedSceneColor = 0,
        kAlbedo,
        kNormal,
        kDepth,
        kMetallicRoughness,
        kSkyIrradiance,
        kDirectLighting,
        kDirectDiffuseLighting,
        kIndirectIrradiance,
        kIndirectLighting,
        kCount
    };

    struct SceneRendererSettings
    {
        ReSTIRSSDOSettings ReSTIRSSDOSettings;
        ReSTIRSSGISettings ReSTIRSSGISettings;
        RenderMode renderMode = RenderMode::kResolvedSceneColor;
    };

    class SceneRenderer
    {
    public:
        friend class GfxModule;

        static SceneRenderer* get();
        virtual ~SceneRenderer();

        void render(Scene* scene, SceneView& sceneView);

        void renderSceneDepth(GHDepthTexture* outDepth, Scene* scene, const SceneView::State& viewState);
        void renderSceneGBuffer(Scene* scene, SceneView& sceneView);
        void renderSceneDirectLighting(Scene* scene, SceneView& sceneView);
        void renderSceneIndirectLighting(Scene* scene, SceneView& sceneView);

        void tonemapping(GHTexture2D* dstTexture, GHTexture2D* srcTexture);
    private:
        SceneRenderer();

        static SceneRenderer* s_renderer;
    public:
        SceneRendererSettings m_settings;
    };
} 