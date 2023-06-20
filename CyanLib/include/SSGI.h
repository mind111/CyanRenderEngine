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
    class SceneRender;

    class SSGIRenderer
    {
    public:
        struct Settings
        {
            i32 kTracingStopMipLevel = 1;
            i32 kMaxNumIterationsPerRay = 64;
        };

        SSGIRenderer();
        ~SSGIRenderer();

        // ao and bent normal
        void renderAO(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);

        // diffuse GI
        void renderDiffuse(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        void stochasticIndirectIrradiance(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);

        // reflection
        void renderReflection(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
    private:
        Settings m_settings;
        std::shared_ptr<Texture2D> m_blueNoise_1024x1024_RGBA = nullptr;
        std::array<std::shared_ptr<Texture2D>, 8> m_blueNoise_16x16_R;
    };
}
