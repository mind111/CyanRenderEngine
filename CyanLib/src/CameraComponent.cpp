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
                // TODO: learn quaternion properly and improve the rotation code below! 
                // yaw first
                {
                    f32 yawAngle = glm::radians(m_yawOneFrame);
                    glm::mat4 m = t.toMatrix();
                    glm::vec3 forward = -m[2];
                    const glm::vec3 yAxis(0.f, 1.f, 0.f);
                    glm::quat qYaw(glm::cos(yawAngle * .5f), yAxis * glm::sin(yawAngle * .5f));
                    forward = glm::normalize(glm::rotate(qYaw, glm::vec4(forward, 0.f)));
                    glm::vec3 right = glm::normalize(glm::cross(forward, camera->m_worldUp));
                    glm::vec3 up = glm::normalize(glm::cross(right, forward));
                    glm::mat4 yawRotationMatrix(glm::vec4(right, 0.f), glm::vec4(up, 0.f), glm::vec4(-forward, 0.f), glm::vec4(0.f, 0.f, 0.f, 1.f));
                    t.rotation = yawRotationMatrix;
                    m_cameraComponent->setLocalTransform(t);
                }
                // then pitch
                {
                    f32 pitchAngle = glm::radians(m_pitchOneFrame);
                    glm::mat4 m = t.toMatrix();
                    glm::vec3 forward = -m[2];
                    glm::quat qPitch(glm::cos(pitchAngle * .5f), camera->right() * glm::sin(pitchAngle * .5f));
                    forward = glm::normalize(glm::rotate(qPitch, glm::vec4(forward, 0.f)));
                    glm::vec3 right = glm::normalize(glm::cross(forward, camera->m_worldUp));
                    glm::vec3 up = glm::normalize(glm::cross(right, forward));
                    glm::mat4 pitchRotationMatrix(glm::vec4(right, 0.f), glm::vec4(up, 0.f), glm::vec4(-forward, 0.f), glm::vec4(0.f, 0.f, 0.f, 1.f));
                    t.rotation = pitchRotationMatrix;
                    m_cameraComponent->setLocalTransform(t);
                }
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
            }
            t.translation += m_velocity;
            m_cameraComponent->setLocalTransform(t);
        }

        // reset states
        m_yawOneFrame = 0.f;
        m_pitchOneFrame = 0.f;
        m_velocity = glm::vec3(0.f);
    }
}
