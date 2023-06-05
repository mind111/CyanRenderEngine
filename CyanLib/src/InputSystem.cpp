#include "InputSystem.h"
#include "CyanEngine.h"

namespace Cyan
{
    static void onKeyEvent(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);
    static void onMouseCursorEvent(GLFWwindow* window, f64 x, f64 y);
    static void onMouseButtonEvent(GLFWwindow* window, i32 button, i32 action, i32 mods);
    static void onMouseWheelEvent(GLFWwindow* window, f64 xOffset, f64 yOffset);

    InputSystem* Singleton<InputSystem>::singleton = nullptr;
    InputSystem::InputSystem()
    {

    }

    void InputSystem::initialize()
    {
        GLFWwindow* window = Engine::get()->getAppWindow();

        // bind I/O handlers 
        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, onMouseButtonEvent);
        glfwSetCursorPosCallback(window, onMouseCursorEvent);
        glfwSetScrollCallback(window, onMouseWheelEvent);
        glfwSetKeyCallback(window, onKeyEvent);
    }

    void InputSystem::update()
    {
        glfwPollEvents();
    }
    
    void InputSystem::deinitialize()
    {

    }

    void InputSystem::registerKeyEventListener(char key, const KeyEventListener& func)
    {
        auto& listeners = m_keyEventListeners[key];
        listeners.push_back(func);
    }

    void InputSystem::registerMouseCursorEventListener(const MouseCursorEventListener& listener)
    {
        m_mouseCursorListeners.push_back(listener);
    }

    void InputSystem::registerMouseButtonEventListener(const MouseButtonEventListener& listener)
    {
        m_mouseButtonListeners.push_back(listener);
    }

    static void onKeyEvent(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods)
    {
        auto InputSystem = InputSystem::get();
        switch (key)
        {
            case GLFW_KEY_W:
            {
                char key = 'W';
                const auto& listeners = InputSystem->m_keyEventListeners[key];
                for (const auto& listener : listeners)
                {
                    switch (action)
                    {
                    case GLFW_PRESS: listener(KeyEvent{ key, InputAction::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, InputAction::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, InputAction::kRepeat }); break;
                    default: assert(0);
                    }
                }
                break;
            }
            case GLFW_KEY_A:
            {
                char key = 'A';
                const auto& listeners = InputSystem->m_keyEventListeners[key];
                for (const auto& listener : listeners)
                {
                    switch (action)
                    {
                    case GLFW_PRESS: listener(KeyEvent{ key, InputAction::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, InputAction::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, InputAction::kRepeat }); break;
                    default: assert(0);
                    }
                }
                break;
            }
            case GLFW_KEY_S:
            {
                char key = 'S';
                const auto& listeners = InputSystem->m_keyEventListeners[key];
                for (const auto& listener : listeners)
                {
                    switch (action)
                    {
                    case GLFW_PRESS: listener(KeyEvent{ key, InputAction::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, InputAction::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, InputAction::kRepeat }); break;
                    default: assert(0);
                    }
                }
                break;
            }
            case GLFW_KEY_D:
            {
                char key = 'D';
                const auto& listeners = InputSystem->m_keyEventListeners[key];
                for (const auto& listener : listeners)
                {
                    switch (action)
                    {
                    case GLFW_PRESS: listener(KeyEvent{ key, InputAction::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, InputAction::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, InputAction::kRepeat }); break;
                    default: assert(0);
                    }
                }
                break;
            }
        }
    }

    static void onMouseCursorEvent(GLFWwindow* window, f64 x, f64 y)
    {
        auto inputSystem = InputSystem::get();
        if (!inputSystem->m_mouseState.bInited)
        {
            inputSystem->m_mouseState.cursorPosition = glm::vec2(x, y);
            inputSystem->m_mouseState.cursorDelta = glm::vec2(0.f);
            inputSystem->m_mouseState.bInited = true;
            return;
        }
        glm::vec2 newMouseCursorPos = glm::vec2(x, y);
        inputSystem->m_mouseState.cursorDelta = newMouseCursorPos - inputSystem->m_mouseState.cursorPosition;
        inputSystem->m_mouseState.cursorPosition = newMouseCursorPos;

        // notify listeners
        for (auto& listener : inputSystem->m_mouseCursorListeners)
        {
            listener(inputSystem->m_mouseState);
        }
    }

    static void onMouseButtonEvent(GLFWwindow* window, i32 button, i32 action, i32 mods)
    {
        auto inputSystem = InputSystem::get();

        switch (button)
        {
            case GLFW_MOUSE_BUTTON_LEFT: 
            {
                switch (action)
                {
                case GLFW_PRESS: inputSystem->m_mouseState.mouseLeftButtonState = MouseEvent::MouseButtonState::kPress; break;
                case GLFW_REPEAT: inputSystem->m_mouseState.mouseLeftButtonState = MouseEvent::MouseButtonState::kRepeat; break;
                case GLFW_RELEASE: inputSystem->m_mouseState.mouseLeftButtonState = MouseEvent::MouseButtonState::kRelease; break;
                default:
                    assert(0);
                }
            } break;
            case GLFW_MOUSE_BUTTON_RIGHT: 
            {
                switch (action)
                {
                case GLFW_PRESS: inputSystem->m_mouseState.mouseRightButtonState = MouseEvent::MouseButtonState::kPress; break;
                case GLFW_REPEAT: inputSystem->m_mouseState.mouseRightButtonState = MouseEvent::MouseButtonState::kRepeat; break;
                case GLFW_RELEASE: inputSystem->m_mouseState.mouseRightButtonState = MouseEvent::MouseButtonState::kRelease; break;
                default:
                    assert(0);
                }
            } break;
        }

        // notify listeners
        for (auto& listener : inputSystem->m_mouseButtonListeners)
        {
            listener(inputSystem->m_mouseState);
        }
    }

    static void onMouseWheelEvent(GLFWwindow* window, f64 xOffset, f64 yOffset)
    {
    }
}

