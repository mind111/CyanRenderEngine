#include "InputSystem.h"
#include "CyanEngine.h"

namespace Cyan
{
    static void mouseButtonCallback(GLFWwindow* window, i32 button, i32 action, i32 mods);
    static void mouseWheelCallback(GLFWwindow* window, f64 xOffset, f64 yOffset);
    static void onMouseCursorEvent(GLFWwindow* window, f64 x, f64 y);
    static void onKeyEvent(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);

    InputSystem* Singleton<InputSystem>::singleton = nullptr;
    InputSystem::InputSystem()
    {

    }

    void InputSystem::initialize()
    {
        GLFWwindow* window = Engine::get()->getAppWindow();

        // bind I/O handlers 
        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, onMouseCursorEvent);
        glfwSetScrollCallback(window, mouseWheelCallback);
        glfwSetKeyCallback(window, onKeyEvent);

#if 0
        // hook up event listeners
        m_IOEventDispatcher->addEventListener<MouseCursorEvent>([this](f64 cursorX, f64 cursorY) { 
            if (m_mouseCursorState.x < 0.f || m_mouseCursorState.y < 0.f)
            {
                m_mouseCursorState.x = cursorX;
                m_mouseCursorState.y = cursorY;
                return;
            }
            m_mouseCursorState.dx = cursorX - m_mouseCursorState.x;
            m_mouseCursorState.dy = cursorY - m_mouseCursorState.y;
            m_mouseCursorState.x = cursorX;
            m_mouseCursorState.y = cursorY;
        });
#endif
    }

    void InputSystem::update()
    {
        glfwPollEvents();
    }
    
    void InputSystem::deinitialize()
    {

    }

    void mouseButtonCallback(GLFWwindow* window, i32 button, i32 action, i32 mods)
    {
    }

    void mouseWheelCallback(GLFWwindow* window, f64 xOffset, f64 yOffset)
    {
    }

    void InputSystem::registerKeyListener(char key, const std::function<void(const KeyEvent&)>& func)
    {
        auto& listeners = m_keyEventListeners[key];
        listeners.push_back(func);
    }

    void InputSystem::registerMouseCursorListener(const MouseCursorEventListener& listener)
    {
        m_mouseCursorListeners.push_back(listener);
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
                    case GLFW_PRESS: listener(KeyEvent{ key, Action::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, Action::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, Action::kRepeat }); break;
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
                    case GLFW_PRESS: listener(KeyEvent{ key, Action::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, Action::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, Action::kRepeat }); break;
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
                    case GLFW_PRESS: listener(KeyEvent{ key, Action::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, Action::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, Action::kRepeat }); break;
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
                    case GLFW_PRESS: listener(KeyEvent{ key, Action::kPress }); break;
                    case GLFW_RELEASE: listener(KeyEvent{ key, Action::kRelease }); break;
                    case GLFW_REPEAT: listener(KeyEvent{ key, Action::kRepeat }); break;
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
        if (!inputSystem->m_mouseCursorState.bInited)
        {
            inputSystem->m_mouseCursorState.pos = glm::vec2(x, y);
            inputSystem->m_mouseCursorState.bInited = true;
            return;
        }
        glm::vec2 newMouseCursorPos = glm::vec2(x, y);
        inputSystem->m_mouseCursorState.delta = newMouseCursorPos - inputSystem->m_mouseCursorState.pos;
        inputSystem->m_mouseCursorState.pos = newMouseCursorPos;

        // notify listeners
        if (glm::length(inputSystem->m_mouseCursorState.delta) > 0.f)
        {
            for (auto& listener : inputSystem->m_mouseCursorListeners)
            {
                listener(inputSystem->m_mouseCursorState);
            }
        }
    }
}

