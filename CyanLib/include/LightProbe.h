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
        LightProbe(const glm::vec3& position, u32 resolution);
        virtual ~LightProbe() { }

        void setScene(Scene* scene);
        virtual void buildFrom(GfxTextureCube* srcCubemap) = 0;

        Scene* m_scene = nullptr;
        glm::vec3 m_position = glm::vec3(0.f);
        u32 m_resolution;
        GfxTextureCube* m_srcCubemap = nullptr;
    };

    class IrradianceProbe : public LightProbe 
    {
    public:
        IrradianceProbe(const glm::vec3& position, u32 resolution);
        ~IrradianceProbe() { }

        virtual void buildFrom(GfxTextureCube* srcCubemap) override;

        static constexpr u32 kNumSamplesInTheta = 32u;
        static constexpr u32 kNumSamplesInPhi = 32u;
        std::unique_ptr<GfxTextureCube> m_irradianceCubemap = nullptr;
    };

    class ReflectionProbe: public LightProbe
    {
    public:
        ReflectionProbe(const glm::vec3& position, u32 resolution);
        ~ReflectionProbe() { }

        /* LightProbe Interface */
        virtual void buildFrom(GfxTextureCube* srcCubemap) override;

        static std::unique_ptr<GfxTexture2D> s_BRDFLookupTexture;
        static const u32 kNumMips = 11; 
        std::unique_ptr<GfxTextureCube> m_reflectionCubemap = nullptr;
    private:
        static void buildBRDFLookupTexture();
    };

    extern const glm::vec3 cameraFacingDirections[6];
    extern const glm::vec3 worldUps[6];
}