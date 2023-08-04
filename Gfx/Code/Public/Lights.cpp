#include "Lights.h"
#include "Shader.h"
#include "GfxContext.h"
#include "RenderPass.h"

namespace Cyan 
{
    Light::Light(const glm::vec3& color, f32 intensity)
        : m_color(color), m_intensity(intensity)
    {

    }

    Light::~Light()
    {

    }

    DirectionalLight::DirectionalLight(const glm::vec3& color, f32 intensity, const glm::vec3& direction)
        : Light(color, intensity), m_direction(direction)
    {
    }

#if 0
    SkyLight::SkyLight(SkyLightComponent* skyLightComponent)
        : Light(skyLightComponent->getColor(), skyLightComponent->getIntensity())
    {
        // create cubemap
        {
            u32 numMips = std::log2(cubemapCaptureResolution);
            GfxTextureCube::Spec spec(cubemapCaptureResolution, numMips, PF_RGB16F);
            SamplerCube sampler;
            sampler.minFilter = FM_TRILINEAR;
            sampler.magFilter = FM_BILINEAR;
            sampler.wrapS = WM_CLAMP;
            sampler.wrapT = WM_CLAMP;
            m_cubemap.reset(GfxTextureCube::create(spec, sampler));
        }
        // create irradiance probe
        {
            m_irradianceProbe.reset(new IrradianceProbe(glm::vec3(0.f), irradianceResolution));
        }
        // create reflection probe
        {
            m_reflectionProbe.reset(new ReflectionProbe(glm::vec3(0.f), reflectionResolution));
        }
    }

    SkyLight::~SkyLight()
    {

    }

    void SkyLight::buildFromHDRI(Texture2D* HDRI)
    {
        if (HDRI != nullptr)
        {
            // 1. convert 2D HDRI (equirectangular map) to cubemap
            {
                Renderer::buildCubemapFromHDRI(m_cubemap.get(), HDRI->getGfxResource());
            }
            // 2. build light probes
            {
                m_irradianceProbe->buildFrom(m_cubemap.get());
                m_reflectionProbe->buildFrom(m_cubemap.get());
            }
        }
    }

    void SkyLight::buildFromScene(Scene* scene)
    {
        // 1. take a cubemap scene capture

        // 2. build light probes
    }
#endif
}