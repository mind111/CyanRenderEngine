#include "AssetManager.h"
#include "AssetImporter.h"
#include "CyanRenderer.h"
#include "Camera.h"
#include "Lights.h"
#include "RenderPass.h"
#include "LightComponents.h"

namespace Cyan 
{
    DirectionalLight::DirectionalLight(DirectionalLightComponent* directionalLightComponent)
        : m_directionalLightComponent(directionalLightComponent)
    {
        m_color = m_directionalLightComponent->getColor();
        m_intensity = m_directionalLightComponent->getIntensity();
        m_direction = m_directionalLightComponent->getDirection();
        m_csm = std::make_unique<CascadedShadowMap>(this);
    }
}