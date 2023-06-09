#include "World.h"
#include "Scene.h"
#include "Entity.h"
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

    f32 LightComponent::getIntensity()
    {
        return m_intensity;
    }

    void LightComponent::setColor(const glm::vec3& color)
    {
        m_color = color;
    }

    void LightComponent::setIntensity(const f32 intensity)
    {
        m_intensity = intensity;
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

    void DirectionalLightComponent::setColor(const glm::vec3& color)
    {
        LightComponent::setColor(color);
        m_directionalLight->m_color = color;
    }

    void DirectionalLightComponent::setIntensity(const f32 intensity)
    {
        LightComponent::setIntensity(intensity);
        m_directionalLight->m_intensity = intensity;
    }

    void DirectionalLightComponent::setDirection(const glm::vec3& direction)
    {
        // todo: find a rotation matrix that will drive m_direction to direction, and  use this derived transform delta to update the component's transform
    }

    SkyLightComponent::SkyLightComponent(const char* name, const Transform& localTransform)
        : LightComponent(name, localTransform)
    {
        m_skyLight = std::make_unique<SkyLight>(this);
    }

    SkyLightComponent::~SkyLightComponent()
    {

    }

    void SkyLightComponent::setOwner(Entity* owner)
    {
        LightComponent::setOwner(owner);
        Scene* scene = m_owner->getWorld()->getScene();
        scene->addSkyLight(m_skyLight.get());
    }

    void SkyLightComponent::setCaptureMode(const CaptureMode& captureMode)
    {
        m_captureMode = captureMode;
    }

    void SkyLightComponent::setHDRI(std::shared_ptr<Texture2D> HDRI)
    {
        m_HDRI = HDRI;
    }

    void SkyLightComponent::captureHDRI()
    {
        if (m_HDRI != nullptr)
        {
            m_skyLight->buildFromHDRI(m_HDRI.get());
        }
    }

    void SkyLightComponent::captureScene()
    {
        Scene* scene = getOwner()->getWorld()->getScene();
        if (scene != nullptr)
        {
            m_skyLight->buildFromScene(scene);
        }
    }

    void SkyLightComponent::setColor(const glm::vec3& color)
    {
        LightComponent::setColor(color);
        m_skyLight->m_color = color;
    }

    void SkyLightComponent::setIntensity(const f32 intensity)
    {
        LightComponent::setIntensity(intensity);
        m_skyLight->m_intensity = intensity;
    }

    void SkyLightComponent::capture()
    {
        switch (m_captureMode)
        {
        case CaptureMode::kScene: captureScene(); break;
        case CaptureMode::kHDRI: captureHDRI(); break;
        default: assert(0);
        }
    }
}
