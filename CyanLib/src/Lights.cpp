#include "Lights.h"
#include "LightComponents.h"
#include "Shader.h"
#include "AssetManager.h"
#include "GfxContext.h"
#include "CyanRenderer.h"
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

    DirectionalLight::DirectionalLight(DirectionalLightComponent* directionalLightComponent)
        : Light(directionalLightComponent->getColor(), directionalLightComponent->getIntensity()), m_directionalLightComponent(directionalLightComponent)
    {
        m_direction = m_directionalLightComponent->getDirection();
        m_csm = std::make_unique<CascadedShadowMap>(this);
    }

    SkyLight::SkyLight(SkyLightComponent* skyLightComponent)
        : Light(skyLightComponent->getColor(), skyLightComponent->getIntensity())
    {
        u32 numMips = std::log2(cubemapCaptureResolution);
        GfxTextureCube::Spec spec(cubemapCaptureResolution, numMips, PF_RGB16F);
        SamplerCube sampler;
        sampler.minFilter = FM_TRILINEAR;
        sampler.magFilter = FM_BILINEAR;
        sampler.wrapS = WM_CLAMP;
        sampler.wrapT = WM_CLAMP;
        m_cubemap.reset(GfxTextureCube::create(spec, sampler));
        m_irradianceProbe.reset(new IrradianceProbe(glm::vec3(0.f), irradianceResolution));
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
                // m_reflectionProbe->buildFrom(m_cubemap.get());
            }
        }
    }

    void SkyLight::buildFromScene(Scene* scene)
    {
        // 1. take a cubemap scene capture

        // 2. build light probes
    }

}