#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Common.h"
#include "RenderTexture.h"

namespace Cyan
{
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

            GfxTexture2DArray* position = nullptr;
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
        i32 numIterations = 32u;
        const i32 kMaxNumReuseSamples = 32;
        i32 numReuseSamples = 32;
        f32 reuseKernelRadius = .01f;
        f32 indirectIrradianceNormalErrTolerance = -.1f;
        glm::vec2 debugPixelCoord = glm::vec2(.5f);

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
        void renderScreenSpaceRayTracedIndirectIrradiance(RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene);
        void renderReflection() { };

        // used for SSAO temporal filtering
        GLuint depthBilinearSampler = -1;
        GLuint SSAOHistoryBilinearSampler = -1;
        Texture2D* blueNoiseTextures_16x16[8] = { nullptr };

        // only instance
        static std::unique_ptr<SSGI> s_instance;
    };
}
