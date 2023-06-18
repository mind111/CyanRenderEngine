#pragma once

#include <utility>

#include "SceneCamera.h"
#include "SceneComponent.h"

namespace Cyan
{
    class CameraComponent : public SceneComponent
    {
    public:
        CameraComponent(const char* name, const Transform& local, const glm::uvec2& resolution);
        ~CameraComponent() { }

        /* SceneComponent Interface*/
        virtual void onTransformUpdated() override;

        SceneCamera* getSceneCamera() { return m_sceneCamera.get(); }

        void turnOn();
        void turnOff();

    protected:
        std::unique_ptr<SceneCamera> m_sceneCamera = nullptr;
    };

    class CameraControllerComponent : public Component
    {
    public:
        CameraControllerComponent(const char* name, CameraComponent* cameraComponent);
        ~CameraControllerComponent() { }

        virtual void update() override;

        void turnOn();
        void turnOff();

    protected:
        bool bEnabled = true;
        f32 m_yawOneFrame = 0.f, m_pitchOneFrame = 0.f; // in degrees
        f32 m_yaw = 0.f, m_pitch = 0.f;
        glm::vec3 m_velocity = glm::vec3(0.f);
        static constexpr f32 kMaxSpeed = 0.2f;
        f32 m_speed = 0.15f;
        bool bMouseRightButtonPressed = false;
        bool bWKeyPressed = false, bAKeyPressed = false, bSKeyPressed = false, bDKeyPressed = false;
        CameraComponent* m_cameraComponent = nullptr;
    };
}
