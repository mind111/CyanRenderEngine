#pragma once

#include "Shader.h"
#include "RenderPass.h"
#include "Bounds.h"

namespace Cyan
{
#define ENGINE_TEXTURE_PATH "C:/dev/cyanRenderEngine/Gfx/Texture/"

    class GHTexture2D;
    class GfxStaticSubMesh;
    class Skybox;

    using RenderTargetSetupFunc = const std::function<void(RenderPass&)>;
    using ShaderSetupFunc = const std::function<void(GfxPipeline* p)>;

    class RenderingUtils
    {
    public:
        struct NoiseTextures
        {
            const char* blueNoise16x16_0_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_0.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_0 = nullptr;

            const char* blueNoise16x16_1_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_1.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_1 = nullptr;

            const char* blueNoise16x16_2_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_2.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_2 = nullptr;

            const char* blueNoise16x16_3_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_3.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_3 = nullptr;

            const char* blueNoise16x16_4_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_4.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_4 = nullptr;

            const char* blueNoise16x16_5_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_5.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_5 = nullptr;

            const char* blueNoise16x16_6_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_6.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_6 = nullptr;

            const char* blueNoise16x16_7_Path = ENGINE_TEXTURE_PATH "/Noise/BN_16X16_R_7.png";
            std::unique_ptr<GHTexture2D> blueNoise16x16_R_7 = nullptr;

            const char* blueNoise1024x1024_RGBA_Path = ENGINE_TEXTURE_PATH "/Noise/BN_1024X1024_RGBA.png";
            std::unique_ptr<GHTexture2D> blueNoise1024x1024_RGBA = nullptr;
        };

        ~RenderingUtils();

        GFX_API static RenderingUtils* get();
        void initialize();
        void deinitialize();

        GfxStaticSubMesh* getUnitQuadMesh() { return s_unitQuadMesh; }
        GfxStaticSubMesh* getUnitCubeMesh() { return s_unitCubeMesh; }
        NoiseTextures& getNoiseTextures() { return s_noiseTextures; }

        GFX_API static void renderSkybox(GHTexture2D* dstColorTexture, GHDepthTexture* dstDepthTexture, Skybox* skybox, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, i32 mipLevel);
        GFX_API static void tonemapping(GHTexture2D* dstTexture, GHTexture2D* srcTexture);
        GFX_API static void postprocessing(GHTexture2D* dstTexture, GHTexture2D* srcTexture);
        GFX_API static void renderScreenPass(const glm::uvec2& renderResolution, const RenderTargetSetupFunc& renderTargetSetupFunc, GfxPipeline* p, ShaderSetupFunc& shaderSetupFunc, bool bDepthTest = false);
        GFX_API static void copyTexture(GHTexture2D* dstTexture, u32 dstMip, GHTexture2D* srcTexture, u32 srcMip);
        GFX_API static void copyDepthTexture(GHDepthTexture* dstTexture, GHDepthTexture* srcTexture);
        GFX_API static void clearTexture2D(GHTexture2D* textureToClear, const glm::vec4& clearColor);
        GFX_API static void clearDepthTexture(GHDepthTexture* depthTextureToClear, f32 depthClearValue);

        // debug drawing helpers
        GFX_API static void drawScreenSpacePoint(GHTexture2D* dstTexture, const glm::vec3& p, const glm::vec4& color);
        GFX_API static void drawWorldSpacePoint(GHTexture2D* dstTexture, GHDepthTexture* depthTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& p, const glm::vec4& color, f32 size);
        GFX_API static void drawWorldSpaceLine(GHTexture2D* dstTexture, GHDepthTexture* depthTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
        GFX_API static void drawAABB(GHTexture2D* dstTexture, GHDepthTexture* depthTexture, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const AxisAlignedBoundingBox3D& aabb, const glm::vec4& color);

        /**
         * Blit a 2D texture to the default framebuffer
         */
        GFX_API static void renderToBackBuffer(GHTexture2D* srcTexture);
        GFX_API static void renderToBackBuffer(GHTexture2D* srcTexture, const Viewport& viewport);

        GFX_API static void dispatchComputePass(ComputePipeline* cp, std::function<void(ComputePipeline*)>&& passLambda, i32 threadGroupSizeX, i32 threadGroupSizeY, i32 threadGroupSizeZ);

        // misc
        GFX_API static void buildCubemapFromHDRI(GHTextureCube* outTexCube, GHTexture2D* HDRITex);
    private:
        RenderingUtils();

        static GfxStaticSubMesh* s_unitQuadMesh;
        static GFX_API GfxStaticSubMesh* s_unitCubeMesh;
        static NoiseTextures s_noiseTextures;

        static RenderingUtils* s_instance;
    };
}