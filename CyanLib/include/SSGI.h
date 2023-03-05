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

        // basic brute force hierarchical tracing without spatial ray reuse
        void render(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer);
        // spatial reuse
        void renderEx(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer);
        // todo: spatio-temporal reuse

        static const u32 kNumSamples = 8u;
        static const u32 kNumIterations = 64u;
        i32 numReuseSamples = 8;
        f32 reuseKernelRadius = .05f;

        glm::uvec2 resolution;
        HitBuffer hitBuffer;
    private:
        // explicitly hide constructor to disallow direct construction
        SSGI(const glm::uvec2& inRes);

        // only instance
        static std::unique_ptr<SSGI> s_instance;
    };
}
