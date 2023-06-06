#pragma once

#include "Common.h"
#include "SceneComponent.h"
#include "Lights.h"

namespace Cyan
{
    class LightComponent : public SceneComponent
    {
    public:
        const glm::vec3& getColor();

    protected:
        LightComponent(const char* name, const Transform& localTransform);
        ~LightComponent() { }

        glm::vec3 m_color = glm::vec3(0.88f, 0.77f, 0.65f);
        f32 m_intensity = 1.f;
    };

    class DirectionalLightComponent : public LightComponent
    {
    public:
        DirectionalLightComponent(const char* name, const Transform& localTransform);
        ~DirectionalLightComponent() { }

        f32 getIntensity();
        void setColor(const glm::vec3& color);
        void setIntensity(const f32 intensity);

        virtual void onTransformUpdated() override;

        DirectionalLight* getDirectionalLight();
        const glm::vec3& getDirection();

        /**
         * setDirection() will update direction as well as updating the transform so that this component's facing
           direction is always aligned with the light direction. Updating this component's transform will also affect
           its light direction.
         */
        void setDirection(const glm::vec3& direction);

    protected:
        glm::vec3 m_direction = glm::normalize(glm::vec3(1.f));
        std::unique_ptr<DirectionalLight> m_directionalLight = nullptr;
    };

    class SkyLightComponent : public LightComponent
    {
    };

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
