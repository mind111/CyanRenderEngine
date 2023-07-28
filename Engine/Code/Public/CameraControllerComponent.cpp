#include "CameraControllerComponent.h"
#include "InputManager.h"

namespace Cyan
{
    CameraControllerComponent::CameraControllerComponent(const char* name, SceneCameraComponent* cameraComponent)
        : Component(name), m_cameraComponent(cameraComponent)
    {
        assert(m_cameraComponent != nullptr);
        auto inputManager = InputManager::get();
        inputManager->registerKeyEventListener('W', [this](LowLevelKeyEvent* keyEvent) {
                switch (keyEvent->action)
                {
                case InputAction::kPress: bWKeyPressed = true; break;
                case InputAction::kRelease: bWKeyPressed = false; break;
                case InputAction::kRepeat: bWKeyPressed = true; break;
                default: assert(0);
                }
            }
        );

        inputManager->registerKeyEventListener('A', [this](LowLevelKeyEvent* keyEvent) {
                switch (keyEvent->action)
                {
                case InputAction::kPress: bAKeyPressed = true; break;
                case InputAction::kRelease: bAKeyPressed = false; break;
                case InputAction::kRepeat: bAKeyPressed = true; break;
                default: assert(0);
                }
            });

        inputManager->registerKeyEventListener('S', [this](LowLevelKeyEvent* keyEvent) {
                switch (keyEvent->action)
                {
                case InputAction::kPress: bSKeyPressed = true; break;
                case InputAction::kRelease: bSKeyPressed = false; break;
                case InputAction::kRepeat: bSKeyPressed = true; break;
                default: assert(0);
                }
            });

        inputManager->registerKeyEventListener('D', [this](LowLevelKeyEvent* keyEvent) {
                switch (keyEvent->action)
                {
                case InputAction::kPress: bDKeyPressed = true; break;
                case InputAction::kRelease: bDKeyPressed = false; break;
                case InputAction::kRepeat: bDKeyPressed = true; break;
                default: assert(0);
                }
            });

        inputManager->registerMouseCursorEventListener([this](LowLevelMouseCursorEvent* cursorEvent) {
                // convert mouse cursor movement to yaw, and pitch rotation
                if (m_cameraComponent != nullptr)
                {
                    auto sceneCamera = m_cameraComponent->getSceneCamera();
                    static const f32 sensitivity = 2.f;
                    f32 kPixelToRotationDegree = 45.f / sceneCamera->m_resolution.y;

                    // rotation around camera up
                    if (glm::abs(cursorEvent->delta.x) >= 1.f)
                    {
                        m_yawOneFrame += sensitivity * kPixelToRotationDegree * - cursorEvent->delta.x;
                    }
                    if (glm::abs(cursorEvent->delta.y) >= 1.f)
                    {
                        // rotation around camera right
                        m_pitchOneFrame += sensitivity * kPixelToRotationDegree * - cursorEvent->delta.y;
                    }
                }
            });

        inputManager->registerMouseButtonEventListener([this](LowLevelMouseButtonEvent* mouseButtonEvent) {
                // convert mouse cursor movement to yaw, and pitch rotation
                if (m_cameraComponent != nullptr)
                {
                    if (mouseButtonEvent->button == MouseButton::kMouseRight)
                    {
                        switch (mouseButtonEvent->action)
                        {
                        case InputAction::kPress: bMouseRightButtonPressed = true; break;
                        case InputAction::kRepeat: bMouseRightButtonPressed = true; break;
                        case InputAction::kRelease: bMouseRightButtonPressed = false; break;
                        default: assert(0);
                        }
                    }
                }
            });
    }

    // todo: does the controller needs to be updated before updating the camera, probably yes
    // todo: handle camera rotation
    void CameraControllerComponent::update()
    {
        if (bEnabled)
        {
            auto sceneCamera = m_cameraComponent->getSceneCamera();
            if (sceneCamera != nullptr)
            {
                const CameraViewInfo& cameraViewInfo = sceneCamera->m_cameraViewInfo;
                auto t = m_cameraComponent->getLocalSpaceTransform();

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
                        glm::vec3 right = glm::normalize(glm::cross(forward, CameraViewInfo::worldUpVector()));
                        glm::vec3 up = glm::normalize(glm::cross(right, forward));
                        glm::mat4 yawRotationMatrix(glm::vec4(right, 0.f), glm::vec4(up, 0.f), glm::vec4(-forward, 0.f), glm::vec4(0.f, 0.f, 0.f, 1.f));
                        t.rotation = yawRotationMatrix;
                        m_cameraComponent->setLocalSpaceTransform(t);
                    }
                    // then pitch
                    {
                        f32 pitchAngle = glm::radians(m_pitchOneFrame);
                        glm::mat4 m = t.toMatrix();
                        glm::vec3 forward = -m[2];
                        glm::quat qPitch(glm::cos(pitchAngle * .5f), cameraViewInfo.localSpaceRight() * glm::sin(pitchAngle * .5f));
                        forward = glm::normalize(glm::rotate(qPitch, glm::vec4(forward, 0.f)));
                        glm::vec3 right = glm::normalize(glm::cross(forward, CameraViewInfo::worldUpVector()));
                        glm::vec3 up = glm::normalize(glm::cross(right, forward));
                        glm::mat4 pitchRotationMatrix(glm::vec4(right, 0.f), glm::vec4(up, 0.f), glm::vec4(-forward, 0.f), glm::vec4(0.f, 0.f, 0.f, 1.f));
                        t.rotation = pitchRotationMatrix;
                        m_cameraComponent->setLocalSpaceTransform(t);
                    }
                }

                // and then handle translation
                glm::vec3 direction = glm::vec3(0.f);
                if (bWKeyPressed)
                {
                    direction += cameraViewInfo.localSpaceForward();
                }
                if (bAKeyPressed)
                {
                    direction += -cameraViewInfo.localSpaceRight();
                }
                if (bSKeyPressed)
                {
                    direction += -cameraViewInfo.localSpaceForward();
                }
                if (bDKeyPressed)
                {
                    direction += cameraViewInfo.localSpaceRight();
                }
                if (glm::length(direction) > 0.f)
                {
                    m_velocity = glm::normalize(direction) * m_speed;
                }
                t.translation += m_velocity;
                m_cameraComponent->setLocalSpaceTransform(t);
            }

            // reset states
            m_yawOneFrame = 0.f;
            m_pitchOneFrame = 0.f;
            m_velocity = glm::vec3(0.f);
        }
    }

    void CameraControllerComponent::turnOn()
    {
        bEnabled = true;
    }

    void CameraControllerComponent::turnOff()
    {
        bEnabled = false;
    }
}
