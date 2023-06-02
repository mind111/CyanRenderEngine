#include "CameraComponent.h"
#include "InputSystem.h"

namespace Cyan
{
    CameraControllerComponent::CameraControllerComponent(const char* name, CameraComponent* cameraComponent)
        : Component(name), m_cameraComponent(cameraComponent)
    {
        assert(m_cameraComponent != nullptr);
        auto InputSystem = InputSystem::get();
        InputSystem->registerKeyListener('W', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case Action::kPress: bWKeyPressed = true; break;
                case Action::kRelease: bWKeyPressed = false; break;
                case Action::kRepeat: bWKeyPressed = true; break;
                default: assert(0);
                }
            });
        InputSystem->registerKeyListener('A', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case Action::kPress: bAKeyPressed = true; break;
                case Action::kRelease: bAKeyPressed = false; break;
                case Action::kRepeat: bAKeyPressed = true; break;
                default: assert(0);
                }
            });
        InputSystem->registerKeyListener('S', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case Action::kPress: bSKeyPressed = true; break;
                case Action::kRelease: bSKeyPressed = false; break;
                case Action::kRepeat: bSKeyPressed = true; break;
                default: assert(0);
                }
            });
        InputSystem->registerKeyListener('D', [this](const KeyEvent& keyEvent) {
                switch (keyEvent.action)
                {
                case Action::kPress: bDKeyPressed = true; break;
                case Action::kRelease: bDKeyPressed = false; break;
                case Action::kRepeat: bDKeyPressed = true; break;
                default: assert(0);
                }
            });
        InputSystem->registerMouseCursorListener([](const MouseCursorEvent& mouseCursorEvent) {

            });
    }

    // todo: does the controller needs to be updated before updating the camera
    void CameraControllerComponent::update()
    {
        m_velocity = glm::vec3(0.f);
        auto camera = m_cameraComponent->getCamera();
        glm::vec3 direction = glm::vec3(0.f);
        if (camera != nullptr)
        {
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
                auto t = m_cameraComponent->getLocalTransform();
                t.m_translate += m_velocity;
                m_cameraComponent->setLocalTransform(t);
            }
        }
    }
}
