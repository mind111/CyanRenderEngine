#pragma once

#include "Texture.h"
#include "Framebuffer.h"

namespace Cyan
{
    class Scene;
    struct PixelPipeline;

    class LightProbe
    {
    public:
        LightProbe(const glm::vec3& position, GfxTextureCube* srcCubemap, const glm::uvec2& resolution);
        // LightProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& resolution);
        virtual ~LightProbe() { }

        void setScene(Scene* scene);
        virtual void build() = 0;
#if 0
        virtual void init();
        virtual void captureScene();
        virtual void debugRender();
#endif

        Scene* m_scene = nullptr;
        glm::vec3 m_position = glm::vec3(0.f);
        glm::vec2 m_resolution;
        GfxTextureCube* m_srcCubemap = nullptr;
    };

    class IrradianceProbe : public LightProbe 
    {
    public:
        IrradianceProbe(const glm::vec3& position, GfxTextureCube* srcCubemap, const glm::uvec2& resolution);
        ~IrradianceProbe() { }
#if 0
        IrradianceProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution, const glm::uvec2& irradianceResolution);
#endif

#if 0
        virtual void debugRender() override;
        void initialize();
        void convolve();
        void build();
        void buildFromCubemap();
#endif
        virtual void build() override;

        static constexpr u32 kNumSamplesInTheta = 32u;
        static constexpr u32 kNumSamplesInPhi = 32u;
        std::unique_ptr<GfxTextureCube> m_irradianceCubemap = nullptr;
    };

#if 0
    struct ReflectionProbe : public LightProbe
    {
        ReflectionProbe(GfxTextureCube* srcCubemapTexture);
        ReflectionProbe(Scene* scene, const glm::vec3& p, const glm::uvec2& sceneCaptureResolution);
        ~ReflectionProbe() { }

        static GfxTexture2D* buildBRDFLookupTexture();

        static GfxTexture2D* getBRDFLookupTexture()
        {
            return s_BRDFLookupTexture;
        }

        void initialize();
        virtual void debugRender() override;
        void convolve();
        void build();
        void buildFromCubemap();

        static GfxTexture2D* s_BRDFLookupTexture;
        static const u32 kNumMips = 11; 
        static PixelPipeline* s_convolveReflectionPipeline;

        std::unique_ptr<GfxTextureCube> m_convolvedReflectionTexture = nullptr;
    };
#endif
    extern const glm::vec3 cameraFacingDirections[6];
    extern const glm::vec3 worldUps[6];
}