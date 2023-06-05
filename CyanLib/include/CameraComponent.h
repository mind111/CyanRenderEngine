#pragma once

#include <utility>

#include "Camera.h"
#include "SceneComponent.h"

namespace Cyan
{
    class CameraComponent : public SceneComponent
    {
    public:
        template <typename ... Args>
        CameraComponent(Args&&... args) 
            : SceneComponent(std::forward<Args>(args)...)
        { }

        ~CameraComponent() { }

        Camera* getCamera() { return m_camera.get(); }

    protected:
        std::unique_ptr<Camera> m_camera = nullptr;
    };

    class CameraControllerComponent : public Component
    {
    public:
        CameraControllerComponent(const char* name, CameraComponent* cameraComponent);
        ~CameraControllerComponent() { }

        virtual void update() override;
    protected:
        f32 m_yawOneFrame = 0.f, m_pitchOneFrame = 0.f; // in degrees
        glm::vec3 m_velocity = glm::vec3(0.f);
        f32 m_speed = 0.02f;
        bool bMouseRightButtonPressed = false;
        bool bWKeyPressed = false, bAKeyPressed = false, bSKeyPressed = false, bDKeyPressed = false;
        CameraComponent* m_cameraComponent = nullptr;
    };
}
