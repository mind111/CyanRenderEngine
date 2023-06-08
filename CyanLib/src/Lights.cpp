#include "Lights.h"
#include "LightComponents.h"

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

    }

    SkyLight::~SkyLight()
    {

    }

}