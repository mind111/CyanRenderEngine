#include "IOSystem.h"
#include "cyanEngine.h"

namespace Cyan
{
    static void mouseButtonCallback(GLFWwindow* window, i32 button, i32 action, i32 mods);
    static void mouseWheelCallback(GLFWwindow* window, f64 xOffset, f64 yOffset);
    static void mouseCursorCallback(GLFWwindow* window, f64 cursorX, f64 cursorY);
    static void keyCallback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods);

    IOSystem* IOSystem::singleton = nullptr;
    IOSystem::IOSystem()
    {
        if (!singleton)
        {
            singleton = this;
        }
        else
        {
            CYAN_ASSERT(0, "Attempted to instantiate multiple instances of IOSystem")
        }
    }

    void IOSystem::initialize()
    {
        GLFWwindow* window = Engine::get()->getAppWindow();

        // bind I/O handlers 
        glfwSetWindowUserPointer(window, this);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, mouseCursorCallback);
        glfwSetScrollCallback(window, mouseWheelCallback);
        glfwSetKeyCallback(window, keyCallback);

        // init event dispatcher
        m_ioEventDispatcher = new EventDispatcher();
        m_ioEventDispatcher->registerEventType<MouseButtonEvent>();
        m_ioEventDispatcher->registerEventType<MouseWheelEvent>();
        m_ioEventDispatcher->registerEventType<MouseCursorEvent>();
        m_ioEventDispatcher->registerEventType<KeyEvent>();

        // hook up event listeners
        m_ioEventDispatcher->addEventListener<MouseCursorEvent>([this](f64 cursorX, f64 cursorY) { 
            m_mouseCursorState.dx = cursorX - m_mouseCursorState.x;
            m_mouseCursorState.dy = cursorY - m_mouseCursorState.y;
            m_mouseCursorState.x = cursorX;
            m_mouseCursorState.y = cursorY;
        });
    }

    void IOSystem::update()
    {
        glfwPollEvents();
        // process event queue
        m_ioEventDispatcher->dispatch();
    }
    
    void IOSystem::finalize()
    {

    }

    void mouseButtonCallback(GLFWwindow* window, i32 button, i32 action, i32 mods)
    {
        IOSystem::get()->enqueIOEvent<MouseButtonEvent>(button, action);
    }

    void mouseWheelCallback(GLFWwindow* window, f64 xOffset, f64 yOffset)
    {
        IOSystem::get()->enqueIOEvent<MouseWheelEvent>(xOffset, yOffset);
    }

    void mouseCursorCallback(GLFWwindow* window, f64 cursorX, f64 cursorY)
    {
        IOSystem::get()->enqueIOEvent<MouseCursorEvent>(cursorX, cursorY);
    }

    void keyCallback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods)
    {
        IOSystem::get()->enqueIOEvent<KeyEvent>(key, action);
    }
}

