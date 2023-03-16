#pragma once

#include "LightProbe.h"
#include "RenderTexture.h"

namespace Cyan
{
    class PixelPipeline;

    struct Skybox
    {
        Skybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution);
        Skybox(const char* name, GfxTextureCube* srcCubemap);

        ~Skybox() { }

        /** note:
        * this render function assumes that certain scene data such as the global view ssbo is already updated and bound
        */
        void render(RenderTexture2D colorBuffer, RenderDepthTexture2D depthBuffer, const glm::mat4& view, const glm::mat4& projection, f32 mipLevel = 0.f);

        static PixelPipeline* s_cubemapSkyPipeline;
        static PixelPipeline* s_proceduralSkyPipeline;

        Texture2D* m_srcHDRITexture = nullptr;
        std::unique_ptr<GfxTextureCube> m_cubemapTexture = nullptr;
    };
}
