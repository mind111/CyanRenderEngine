#pragma once

#include "Shader.h"
#include "RenderPass.h"

namespace Cyan
{
#define ENGINE_TEXTURE_PATH "C:/dev/cyanRenderEngine/Gfx/Texture/"

    class GHTexture2D;
    class GfxStaticSubMesh;

    using RenderTargetSetupFunc = const std::function<void(RenderPass&)>;
    using ShaderSetupFunc = const std::function<void(GfxPipeline* p)>;

    class RenderingUtils
    {
    public:
        struct NoiseTextures
        {
            const char* blueNoise16x16_0_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_0.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_0 = nullptr;

            const char* blueNoise16x16_1_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_1.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_1 = nullptr;

            const char* blueNoise16x16_2_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_2.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_2 = nullptr;

            const char* blueNoise16x16_3_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_3.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_3 = nullptr;

            const char* blueNoise16x16_4_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_4.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_4 = nullptr;

            const char* blueNoise16x16_5_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_5.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_5 = nullptr;

            const char* blueNoise16x16_6_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_6.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_6 = nullptr;

            const char* blueNoise16x16_7_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_7.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_7 = nullptr;
        };

        ~RenderingUtils();

        static RenderingUtils* get();
        void initialize();
        void deinitialize();

        GfxStaticSubMesh* getUnitQuadMesh() { return s_unitQuadMesh; }
        GfxStaticSubMesh* getUnitCubeMesh() { return s_unitCubeMesh; }
        NoiseTextures& getNoiseTextures() { return s_noiseTextures; }

        static void renderScreenPass(const glm::uvec2& renderResolution, const RenderTargetSetupFunc& renderTargetSetupFunc, GfxPipeline* p, ShaderSetupFunc& shaderSetupFunc);

        /**
         * Blit a 2D texture to the default framebuffer
         */
        static void renderToBackBuffer(GHTexture2D* srcTexture);
    private:
        RenderingUtils();

        static GfxStaticSubMesh* s_unitQuadMesh;
        static GfxStaticSubMesh* s_unitCubeMesh;
        static NoiseTextures s_noiseTextures;

        static RenderingUtils* s_instance;
    };
}
