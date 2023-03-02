#pragma once

#include "Texture.h"
#include "RenderTarget.h"


namespace Cyan
{
    struct Scene;
    struct PixelPipeline;
    struct MeshInstance;

    struct LightProbe
    {
        LightProbe(TextureCube* srcCubemapTexture);
        LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution);
        ~LightProbe() { }
        virtual void init();
        virtual void captureScene();
        virtual void debugRender();

        Scene*               scene;
        glm::vec3            position;
        glm::vec2            resolution;
        TextureCube* sceneCapture;
        MeshInstance*        debugSphereMesh;
    };

    struct IrradianceProbe : public LightProbe {
        IrradianceProbe(TextureCube* srcCubemapTexture, const glm::uvec2& irradianceRes);
        IrradianceProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution, const glm::uvec2& irradianceResolution);
        ~IrradianceProbe() { }
        virtual void debugRender() override;

        void initialize();
        void convolve();
        void build();
        void buildFromCubemap();

        static PixelPipeline* s_convolveIrradiancePipeline;
        static const u32 kNumZenithSlice       = 32u;
        static const u32 kNumAzimuthalSlice    = 32u;
        static const u32 kNumRaysPerHemiSphere = 128u;

        glm::vec2 m_irradianceTextureRes;
        std::unique_ptr<TextureCube> m_convolvedIrradianceTexture = nullptr;
    };

    struct ReflectionProbe : public LightProbe
    {
        ReflectionProbe(TextureCube* srcCubemapTexture);
        ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution);
        ~ReflectionProbe() { }

        static GfxTexture2DBindless* buildBRDFLookupTexture();

        static GfxTexture2DBindless* getBRDFLookupTexture()
        {
            return s_BRDFLookupTexture;
        }

        void initialize();
        virtual void debugRender() override;
        void convolve();
        void build();
        void buildFromCubemap();

        static GfxTexture2DBindless* s_BRDFLookupTexture;
        static const u32 kNumMips = 11; 
        static PixelPipeline* s_convolveReflectionPipeline;

        std::unique_ptr<TextureCube> m_convolvedReflectionTexture = nullptr;
    };

    namespace LightProbeCameras
    {
        extern const glm::vec3 cameraFacingDirections[6];
        extern const glm::vec3 worldUps[6];
    }
}