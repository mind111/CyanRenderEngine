#pragma once

#include "LightProbe.h"

namespace Cyan
{
    // todo: decouple skylight from skybox, skylight can be a stand-alone thing
    struct Skybox
    {
        Skybox(const char* name, const char* srcHDRIPath, const glm::uvec2& resolution);
        Skybox(const char* name, TextureCubeRenderable* srcCubemap);

        ~Skybox() { }

        /** note:
        * this render function assumes that certain scene data such as the global view ssbo is already updated and bound
        */
        void render(RenderTarget* renderTarget, const glm::mat4& view, const glm::mat4& projection, f32 mipLevel = 0.f);

        static PixelPipeline* s_cubemapSkyPipeline;
        static PixelPipeline* s_proceduralSkyPipeline;

        Texture2DRenderable* m_srcHDRITexture = nullptr;
        TextureCubeRenderable* m_cubemapTexture = nullptr;
    };
}
