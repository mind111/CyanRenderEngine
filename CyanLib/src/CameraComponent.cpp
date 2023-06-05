#include "CameraComponent.h"
#include "InputSystem.h"

namespace Cyan
{
    CameraControllerComponent::CameraControllerComponent(const char* name, CameraComponent* cameraComponent)
        : Component(name), m_cameraComponent(cameraComponent)
    {
        assert(m_cameraComponent != nullptr);
        auto InputSystem = InputSystem::get();
        InputSystem->registerKeyEventListener('W', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case InputAction::kPress: bWKeyPressed = true; break;
                case InputAction::kRelease: bWKeyPressed = false; break;
                case InputAction::kRepeat: bWKeyPressed = true; break;
                default: assert(0);
                }
            });

        InputSystem->registerKeyEventListener('A', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case InputAction::kPress: bAKeyPressed = true; break;
                case InputAction::kRelease: bAKeyPressed = false; break;
                case InputAction::kRepeat: bAKeyPressed = true; break;
                default: assert(0);
                }
            });

        InputSystem->registerKeyEventListener('S', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case InputAction::kPress: bSKeyPressed = true; break;
                case InputAction::kRelease: bSKeyPressed = false; break;
                case InputAction::kRepeat: bSKeyPressed = true; break;
                default: assert(0);
                }
            });

        InputSystem->registerKeyEventListener('D', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case InputAction::kPress: bDKeyPressed = true; break;
                case InputAction::kRelease: bDKeyPressed = false; break;
                case InputAction::kRepeat: bDKeyPressed = true; break;
                default: assert(0);
                }
            });

        InputSystem->registerMouseCursorEventListener([this](const MouseEvent& mouseEvent) {
                // convert mouse cursor movement to yaw, and pitch rotation
                if (m_cameraComponent != nullptr)
                {
                    auto camera = m_cameraComponent->getCamera();
                    static const f32 sensitivity = 2.f;
                    f32 kPixelToRotationDegree = 45.f / camera->m_renderResolution.y;
                    if (mouseEvent.bInited)
                    {
                        // rotation around camera up
                        if (glm::abs(mouseEvent.cursorDelta.x) >= 1.f)
                        {
                            m_yawOneFrame += sensitivity * kPixelToRotationDegree * -mouseEvent.cursorDelta.x;
                        }
                        if (glm::abs(mouseEvent.cursorDelta.y) >= 1.f)
                        {
                            // rotation around camera right
                            m_pitchOneFrame += sensitivity * kPixelToRotationDegree * -mouseEvent.cursorDelta.y;
                        }
                    }
                }
            });

        InputSystem->registerMouseButtonEventListener([this](const MouseEvent& mouseEvent) {
                // convert mouse cursor movement to yaw, and pitch rotation
                if (m_cameraComponent != nullptr)
                {
                    switch (mouseEvent.mouseRightButtonState) 
                    {
                    case MouseEvent::MouseButtonState::kPress: bMouseRightButtonPressed = true; break;
                    case MouseEvent::MouseButtonState::kRepeat: bMouseRightButtonPressed = true; break;
                    case MouseEvent::MouseButtonState::kRelease: bMouseRightButtonPressed = false; break;
                    default: assert(0);
                    }
                }
            });
    }

    // todo: does the controller needs to be updated before updating the camera, probably yes
    // todo: handle camera rotation
    void CameraControllerComponent::update()
    {
        auto camera = m_cameraComponent->getCamera();
        if (camera != nullptr)
        {
            auto t = m_cameraComponent->getLocalTransform();

            if (bMouseRightButtonPressed)
            {
                // TODO: this camera rotation thing is bugged, not sure why
                // yaw first
                t.rotation = glm::rotate(t.rotation, glm::radians(m_yawOneFrame), camera->up());
                m_cameraComponent->setLocalTransform(t);

#if 0
                glm::vec3 up = camera->up();
                cyanInfo("Camera Up: (%.2f, %.2f, %.2f)", up.x, up.y, up.z);
#endif

                // then pitch
                t.rotation = glm::rotate(t.rotation, glm::radians(m_pitchOneFrame), camera->right());
                m_cameraComponent->setLocalTransform(t);
            }

            // and then handle translation
            glm::vec3 direction = glm::vec3(0.f);
            if (bWKeyPressed)
            {
                direction += camera->forward();
            }
            if (bAKeyPressed)
            {
                direction += -camera->right();
            }
            if (bSKeyPressed)
            {
                direction += -camera->forward();
            }
            if (bDKeyPressed)
            {
                direction += camera->right();
            }
            if (glm::length(direction) > 0.f)
            {
                m_velocity = glm::normalize(direction) * m_speed;
                t.translation += m_velocity;
            }

            m_cameraComponent->setLocalTransform(t);
        }

        // reset states
        m_yawOneFrame = 0.f;
        m_pitchOneFrame = 0.f;
        m_velocity = glm::vec3(0.f);
    }
}
