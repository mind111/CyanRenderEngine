#pragma once

#include "Lights.h"

namespace Cyan
{
    struct ILightComponent : public Component
    {
        /* Component interface */
        virtual const char* getTag() override { return  "ILightComponent"; }
    };

    struct DirectionalLightComponent : public ILightComponent
    {
        /* Component interface */
        virtual void update() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "DirectionalLightComponent"; }

        DirectionalLightComponent() { }
        DirectionalLightComponent(const glm::vec3 direction, const glm::vec4& colorAndIntensity, bool bCastShadow, DirectionalLight::ShadowMap algorithm = DirectionalLight::ShadowMap::kCSM) { 
            switch (algorithm) {
            case DirectionalLight::ShadowMap::kBasic:
                directionalLight = std::make_shared<DirectionalLight>(direction, colorAndIntensity, bCastShadow);
                break;
            case DirectionalLight::ShadowMap::kCSM:
                directionalLight = std::make_shared<CSMDirectionalLight>(direction, colorAndIntensity, bCastShadow);
                break;
            case DirectionalLight::ShadowMap::kVSM:
                break;
            default:
                break;
            }
        }

        const glm::vec3& getDirection() { return directionalLight->direction; }
        const glm::vec4& getColorAndIntensity() { return directionalLight->colorAndIntensity; }

        void setDirection(const glm::vec3& inDirection) { directionalLight->direction = inDirection; }
        void setColorAndIntensity(const glm::vec4& inColorAndIntensity) { directionalLight->colorAndIntensity = inColorAndIntensity; }

        std::shared_ptr<DirectionalLight> directionalLight = nullptr;
    };

    struct SkyLightComponent : public ILightComponent
    {
        /* Component interface */
        virtual void update() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "SkyLightComponent"; }
    private:

    };

    struct PointLightComponent : public ILightComponent
    {
        /* Component interface */
        virtual void update() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "PointLightComponent"; }

        void getPosition() { }
        void getColor() { }

        void setPosition() { }
        void setColor() { }

        PointLight pointLight;
    };
}
