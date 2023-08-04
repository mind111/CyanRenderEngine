#pragma once

#include "Core.h"
#include "SceneView.h"

namespace Cyan
{
    class Scene;
    class SceneRender;
    class SSGIDebugger;

    class SSGIRenderer
    {
    public:
        friend class SSGIDebugger;
        struct Settings
        {
            i32 kTracingStopMipLevel = 1;
            i32 kMaxNumIterationsPerRay = 64;
            bool bNearFieldSSAO = true;
            bool bSSDO = true;
            bool bIndirectIrradiance = true;
            static constexpr f32 kMinIndirectBoost = 0.f;
            static constexpr f32 kMaxIndirectBoost = 10.f;
            f32 indirectBoost= 1.f;
        };

        SSGIRenderer();
        ~SSGIRenderer();

        void renderSceneIndirectLighting(Scene* scene, SceneRender* render, const SceneView::State& viewParameters);
        // virtual void renderUI();

        // ao and bent normal
        void renderAO(SceneRender* render, const SceneView::State& viewParameters);
#if 0
        // diffuse GI
        virtual void renderDiffuseGI(SkyLight* skyLight, SceneRender* render, const SceneView::State& viewParameters);
        // reflection
        virtual void renderReflection(SkyLight* skyLight, SceneRender* render, const SceneView::State& viewParameters);
        // debugging
        void debugDraw(SceneRender* render, const SceneView::State& viewParameters);
#endif

    protected:
        Settings m_settings;
        std::array<GHTexture2D*, 8> m_blueNoise_16x16_R;

        bool bDebugging = false;
        // std::unique_ptr<SSGIDebugger> m_debugger = nullptr;
    };

#if 0
    class SSGIDebugger
    {
    public:
        SSGIDebugger(SSGIRenderer* renderer);
        ~SSGIDebugger();

        void render(SceneRender* render, const SceneView::State& viewParameters);
        void renderUI();

        void debugDrawRay(SceneRender* render, const SceneView::State& viewParameters);
        void debugDrawRayOriginAndOffset(SceneRender* render, const SceneView::State& viewParameters);

        enum class HitResult
        {
            kHit = 0,
            kBackface,
            kHidden,
            kMiss
        };

        struct DebugRay
        {
            glm::vec4 ro = glm::vec4(0.f);
            glm::vec4 screenSpaceRo = glm::vec4(0.f);
            glm::vec4 screenSpaceRoWithOffset = glm::vec4(0.f);
            glm::vec4 screenSpaceRoOffsetT = glm::vec4(0.f);
            glm::vec4 screenSpaceT = glm::vec4(0.f);
            glm::vec4 rd = glm::vec4(0.f);
            glm::vec4 screenSpaceRd = glm::vec4(0.f);
            glm::vec4 n = glm::vec4(0.f);
            glm::vec4 screenSpaceHitPosition = glm::vec4(0.f);
            glm::vec4 worldSpaceHitPosition = glm::vec4(0.f);
            glm::vec4 hitRadiance = glm::vec4(0.f, 0.f, 0.f, 1.f);
            glm::vec4 hitNormal = glm::vec4(0.f);
            glm::vec4 hitResult = glm::vec4(f32(HitResult::kMiss));
        };

        glm::vec2 m_debugRayScreenCoord = glm::vec2(.5f);
        bool m_freezeDebugRay = false;
        std::unique_ptr<ShaderStorageBuffer> m_debugRayBuffer = nullptr;
        DebugRay m_debugRay;

        struct RayMarchingInfo
        {
            glm::vec4 screenSpacePosition = glm::vec4(0.f);
            glm::vec4 worldSpacePosition = glm::vec4(0.f);
            glm::vec4 depthSampleCoord = glm::vec4(0.f);
            glm::vec4 mipLevel = glm::vec4(0.f);
            glm::vec4 minDepth = glm::vec4(0.f);
            glm::vec4 tCell = glm::vec4(0.f);
            glm::vec4 tDepth = glm::vec4(0.f);
        };
        std::unique_ptr<ShaderStorageBuffer> m_rayMarchingInfoBuffer = nullptr;
        std::vector<RayMarchingInfo> m_rayMarchingInfos;

        struct DepthSample
        {
            glm::vec4 sceneDepth;
            glm::vec4 quadtreeMinDepth;
        };
        std::unique_ptr<ShaderStorageBuffer> m_depthSampleBuffer = nullptr;
        DepthSample m_depthSample;
        i32 m_depthSampleLevel = 0;
        glm::vec2 m_depthSampleCoord = glm::vec2(.5f);

        SSGIRenderer* m_SSGIRenderer = nullptr;
    };
#endif

    class ReSTIRSSGIRenderer : public SSGIRenderer
    {
    public:
        ReSTIRSSGIRenderer();
        ~ReSTIRSSGIRenderer();

        // diffuse GI
        // virtual void renderDiffuseGI(SkyLight* skyLight, SceneRender* render, const SceneView::State& viewParameters) override;
        // virtual void renderUI() override;
    private:
        bool bUseJacobian = false;

        // todo: temporal resampling and feeding resampled result into next frame's temporal reservoir is kind of hard to get right
        bool bTemporalResampling = false;

        bool bSpatialResampling = false;
        i32 m_numSpatialReusePass = 2u;
        i32 m_spatialReuseSampleCount = 8u;
        f32 m_spatialReuseRadius = .02f;

        bool bDenoisingPass = false;
        i32 m_numDenoisingPass = 4u;
    };
}
