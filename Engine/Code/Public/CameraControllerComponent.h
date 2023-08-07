#pragma once

#include "Engine.h"
#include "Component.h"
#include "SceneCameraComponent.h"

namespace Cyan
{
    class ENGINE_API CameraControllerComponent : public Component
    {
    public:
        CameraControllerComponent(const char* name, SceneCameraComponent* sceneCameraComponent);
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
        f32 m_speed = 0.01f;
        bool bMouseRightButtonPressed = false;
        bool bWKeyPressed = false, bAKeyPressed = false, bSKeyPressed = false, bDKeyPressed = false;
        SceneCameraComponent* m_cameraComponent = nullptr;
    };
}
