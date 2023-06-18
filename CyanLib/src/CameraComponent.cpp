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
            }
        );

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
                    auto sceneCamera = m_cameraComponent->getSceneCamera();
                    static const f32 sensitivity = 2.f;
                    f32 kPixelToRotationDegree = 45.f / sceneCamera->m_resolution.y;
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
        if (bEnabled)
        {
            auto sceneCamera = m_cameraComponent->getSceneCamera();
            if (sceneCamera != nullptr)
            {
                Camera& camera = sceneCamera->m_camera;
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
                        glm::vec3 right = glm::normalize(glm::cross(forward, Camera::worldUpVector()));
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
                        glm::quat qPitch(glm::cos(pitchAngle * .5f), camera.localSpaceRight() * glm::sin(pitchAngle * .5f));
                        forward = glm::normalize(glm::rotate(qPitch, glm::vec4(forward, 0.f)));
                        glm::vec3 right = glm::normalize(glm::cross(forward, Camera::worldUpVector()));
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
                    direction += camera.localSpaceForward();
                }
                if (bAKeyPressed)
                {
                    direction += -camera.localSpaceRight();
                }
                if (bSKeyPressed)
                {
                    direction += -camera.localSpaceForward();
                }
                if (bDKeyPressed)
                {
                    direction += camera.localSpaceRight();
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

    CameraComponent::CameraComponent(const char* name, const Transform& local, const glm::uvec2& resolution)
        : SceneComponent(name, local)
    {
        m_sceneCamera = std::make_unique<SceneCamera>(resolution);
    }

    void CameraComponent::onTransformUpdated()
    {
        Camera& camera = m_sceneCamera->m_camera;
        // update camera's transform relative to parent
        Transform localSpaceTransform = getLocalSpaceTransform();
        glm::mat4 localTransformMatrix = localSpaceTransform.toMatrix();
        camera.m_localSpaceRight = glm::normalize(localTransformMatrix * glm::vec4(Camera::localRight, 0.f));
        camera.m_localSpaceUp = glm::normalize(localTransformMatrix * glm::vec4(Camera::localUp, 0.f));
        camera.m_localSpaceForward = glm::normalize(localTransformMatrix * glm::vec4(Camera::localForward, 0.f));

        // update camera's global transform
        Transform worldSpaceTransform = getWorldSpaceTransform();
        glm::mat4 worldSpaceMatrix = worldSpaceTransform.toMatrix();
        camera.m_worldSpaceRight = glm::normalize(worldSpaceMatrix * glm::vec4(Camera::localRight, 0.f));
        camera.m_worldSpaceUp = glm::normalize(worldSpaceMatrix * glm::vec4(Camera::localUp, 0.f));
        camera.m_worldSpaceForward = glm::normalize(worldSpaceMatrix * glm::vec4(Camera::localForward, 0.f));
        camera.m_worldSpacePosition = worldSpaceTransform.translation;
    }

    void CameraComponent::turnOn()
    {
        m_sceneCamera->turnOn();
    }

    void CameraComponent::turnOff()
    {
        m_sceneCamera->turnOff();
    }
}
