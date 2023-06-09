#pragma once

#include "Common.h"
#include "SceneComponent.h"
#include "Lights.h"

namespace Cyan
{
    class LightComponent : public SceneComponent
    {
    public:
        virtual ~LightComponent() { }

        const glm::vec3& getColor();
        f32 getIntensity();
        virtual void setColor(const glm::vec3& color);
        virtual void setIntensity(const f32 intensity);

    protected:
        LightComponent(const char* name, const Transform& localTransform);

        glm::vec3 m_color = glm::vec3(0.88f, 0.77f, 0.65f);
        f32 m_intensity = 1.f;
    };

    class DirectionalLightComponent : public LightComponent
    {
    public:
        DirectionalLightComponent(const char* name, const Transform& localTransform);
        ~DirectionalLightComponent() { }

        virtual void onTransformUpdated() override;

        DirectionalLight* getDirectionalLight();
        const glm::vec3& getDirection();
        virtual void setColor(const glm::vec3& color) override;
        virtual void setIntensity(const f32 intensity) override;

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
    public:
        SkyLightComponent(const char* name, const Transform& localTransform);
        ~SkyLightComponent();

        /* SceneComponent Interface */
        virtual void setOwner(Entity* owner);

        /* LightComponent Interface */
        virtual void setColor(const glm::vec3& color) override;
        virtual void setIntensity(const f32 intensity) override;

        enum class CaptureMode
        {
            kHDRI = 0,
            kScene,
            kCount
        };

        void setCaptureMode(const CaptureMode& captureMode);

        /**
         * 'HDRI' should be an equirectangular map
         */
        void setHDRI(std::shared_ptr<Texture2D> HRDI);
        void capture();

    protected:
        void captureHDRI();
        void captureScene();

        static constexpr const char* defaultHDRIPath = "";

        CaptureMode m_captureMode = CaptureMode::kHDRI;
        std::shared_ptr<Texture2D> m_HDRI = nullptr;
        std::unique_ptr<SkyLight> m_skyLight = nullptr;
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
