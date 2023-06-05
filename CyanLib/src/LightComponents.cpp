#include "LightComponents.h"

namespace Cyan
{
    LightComponent::LightComponent(const char* name, const Transform& localTransform)
        : SceneComponent(name, localTransform)
    {
    }

    const glm::vec3& LightComponent::getColor()
    {
        return m_color;
    }

    f32 DirectionalLightComponent::getIntensity()
    {
        return m_intensity;
    }

    void DirectionalLightComponent::setColor(const glm::vec3& color)
    {
        m_color = color;
        m_directionalLight->m_color = color;
    }

    void DirectionalLightComponent::setIntensity(const f32 intensity)
    {
        m_intensity = intensity;
        m_directionalLight->m_intensity = intensity;
    }

    DirectionalLightComponent::DirectionalLightComponent(const char* name, const Transform& localTransform)
        : LightComponent(name, localTransform)
    {
        m_directionalLight = std::make_unique<DirectionalLight>(this);
    }

    void DirectionalLightComponent::onTransformUpdated()
    {

    }

    DirectionalLight* DirectionalLightComponent::getDirectionalLight()
    {
        return m_directionalLight.get();
    }

    const glm::vec3& DirectionalLightComponent::getDirection()
    {
        return m_direction;
    }

    void DirectionalLightComponent::setDirection(const glm::vec3& direction)
    {
        // todo: find a rotation matrix that will drive m_direction to direction, and  use this derived transform delta to update the component's transform
    }
}
