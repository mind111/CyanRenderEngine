#pragma once

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Common.h"
#include "SceneCamera.h"

namespace Cyan
{
#if 0
    struct GBuffer;
    struct HiZBuffer;
    struct GfxTexture2DArray;
    struct RenderableScene;

    // todo: do quality presets
    struct SSGI
    {
        struct HitBuffer
        {
            HitBuffer(u32 inNumLayers, const glm::uvec2& resolution);
            ~HitBuffer() { }

            GfxTexture2DArray* m_position = nullptr;
            GfxTexture2DArray* normal = nullptr;
            GfxTexture2DArray* radiance = nullptr;
            u32 numLayers;
        };

        ~SSGI() { };

        static SSGI* create(const glm::uvec2& inResolution);
        static SSGI* get() { return s_instance.get(); }

        void render(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, const RenderableScene& scene, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer);
        void visualize(GfxTexture2D* outColor, const GBuffer& gBuffer, const RenderableScene& scene, RenderTexture2D inDirectDiffuseBuffer);

        bool bBilateralFiltering = true;

        u32 numSamples = 1u;
        const i32 kMaxNumIterations = 64u;
        i32 numIterations = 64u;
        const i32 kMaxNumReuseSamples = 32;
        i32 numReuseSamples = 32;
        const i32 kNumSpatialReuseIterations = 2;
        i32 numSpatialReuseSamples = 8;
        f32 ReSTIRSpatialReuseKernalRadius = .02f;
        f32 reuseKernelRadius = .01f;
        f32 indirectIrradianceNormalErrTolerance = -.1f;
        glm::vec2 debugPixelCoord = glm::vec2(.5f);
        bool bUseReSTIR = true;

        glm::uvec2 resolution;
        HitBuffer hitBuffer;
    private:
        // explicitly hide constructor to disallow direct construction
        SSGI(const glm::uvec2& inRes);

        /**
         * Reference:
            * HBIL
         */
        void renderHorizonBasedAOAndIndirectIrradiance(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene);

        void renderScreenSpaceRayTracedIndirectIrradiance(RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene);
        void bruteforceScreenSpaceRayTracedIndirectIrradiance(RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene);
        /**
         * Reference:
            *  
         */
        void ReSTIRScreenSpaceRayTracedIndirectIrradiance(RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene);
        void renderReflection() { };

        // used for SSAO temporal filtering
        GLuint depthBilinearSampler = -1;
        GLuint SSAOHistoryBilinearSampler = -1;
        Texture2D* blueNoiseTextures_16x16[8] = { nullptr };

        // only instance
        static std::unique_ptr<SSGI> s_instance;
    };
#endif

    struct GfxTexture2D;
    class Texture2D;
    class SkyLight;
    class SceneRender;
    struct ShaderStorageBuffer;
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

        void renderSceneIndirectLighting(Scene* scene, SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        void renderUI();

        // ao and bent normal
        void renderAO(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        // diffuse GI
        virtual void renderDiffuseGI(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        // reflection
        virtual void renderReflection(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters);

        // debugging
        void debugDraw(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);

    private:
        Settings m_settings;
        bool bDebugging = false;
        std::unique_ptr<SSGIDebugger> m_debugger = nullptr;
        std::shared_ptr<Texture2D> m_blueNoise_1024x1024_RGBA = nullptr;
        std::array<std::shared_ptr<Texture2D>, 8> m_blueNoise_16x16_R;
    };

    class SSGIDebugger
    {
    public:
        SSGIDebugger(SSGIRenderer* renderer);
        ~SSGIDebugger();

        void render(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        void renderUI();

        void debugDrawRay(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        void debugDrawRayOriginAndOffset(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);

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
}
