#pragma once

#include "Lights.h"

namespace Cyan
{
#if 0
    struct ILightComponent : public Component
    {
        ILightComponent(Entity* owner, const char* name) 
            : Component(owner, name)
        { 

        }

        /* Component interface */
        virtual const char* getTag() override { return  "ILightComponent"; }
    };

    struct DirectionalLightComponent : public ILightComponent
    {
        /* Component interface */
        virtual void update() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "DirectionalLightComponent"; }

        DirectionalLightComponent(Entity* owner, const char* name) 
            : ILightComponent(owner, name)
        { 

        }

        DirectionalLightComponent(Entity* owner, const char* name, const glm::vec3 direction, const glm::vec4& colorAndIntensity, bool bCastShadow) 
            : ILightComponent(owner, name)
        { 
            directionalLight = std::make_shared<DirectionalLight>(direction, colorAndIntensity, bCastShadow);
        }

        DirectionalLight* getDirectionalLight() { return directionalLight.get(); }
        const glm::vec3& getDirection() { return directionalLight->direction; }
        const glm::vec4& getColorAndIntensity() { return directionalLight->colorAndIntensity; }

        void setDirection(const glm::vec3& inDirection) { directionalLight->direction = inDirection; }
        void setColorAndIntensity(const glm::vec4& inColorAndIntensity) { directionalLight->colorAndIntensity = inColorAndIntensity; }

        std::shared_ptr<DirectionalLight> directionalLight = nullptr;
    };

    struct SkyLightComponent : public ILightComponent
    {
        SkyLightComponent(Entity* owner, const char* name)
            : ILightComponent(owner, name)
        {

        }

        /* Component interface */
        virtual void update() override { }
        virtual void render() override { }
        virtual const char* getTag() override { return "SkyLightComponent"; }
    private:

    };

    struct PointLightComponent : public ILightComponent
    {
        PointLightComponent(Entity* owner, const char* name)
            : ILightComponent(owner, name)
        {

        }

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
#endif
}
