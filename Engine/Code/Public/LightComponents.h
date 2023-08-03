#pragma once

#include "Core.h"
#include "SceneComponent.h"
#include "Texture.h"
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

        virtual void setOwner(Entity* owner) override;
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

        /* this can only be accessed on the render thread */
        std::unique_ptr<DirectionalLight> m_directionalLight = nullptr;
    };

#if 0
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
        void setHDRI(std::shared_ptr<Texture2D> HDRI);
        void setScene(Scene* scene);
        void capture();

    protected:
        void captureHDRI();
        void captureScene();

        static constexpr const char* defaultHDRIPath = "";

        CaptureMode m_captureMode = CaptureMode::kHDRI;
        Scene* m_scene = nullptr;
        std::shared_ptr<Texture2D> m_HDRI = nullptr;
        std::unique_ptr<SkyLight> m_skyLight = nullptr;
    };
#endif
}
