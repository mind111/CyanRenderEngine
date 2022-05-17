#include <iostream>
#include <chrono>

#include "glew.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "cyanRenderer.h"
#include "window.h"
#include "Shader.h"
#include "Scene.h"
#include "camera.h"
#include "mathUtils.h"
#include "cyanEngine.h"


namespace Cyan
{
    Engine* Engine::singleton = nullptr;

    Engine::Engine(u32 windowWidth, u32 windowHeight)
    {
        if (!singleton)
        {
            singleton = this;
            m_graphicsSystem = new GraphicsSystem(windowWidth, windowHeight);
            m_IOSystem = new IOSystem;
        }
        else
        {
            CYAN_ASSERT(0, "Attempted to instantiate multiple instances of Engine")
        }
    }

    void Engine::initialize()
    {
        // todo: this function should instead call System::initialize() in order to initialize subsystems
        m_graphicsSystem->initialize();
        m_IOSystem->initialize();

        // setup graphics system 
        {
            // Cyan::init();
            // m_graphicsSystem = new Cyan::GraphicsSystem(m_glfwWindow.mpWindow, glm::vec2(m_renderSize.x, m_renderSize.y));
            // Cyan::getCurrentGfxCtx()->setWindow(&m_glfwWindow);
        }
        // init member variables
        {
            cursorX = -1.0;
            cursorY = -1.0;
            cursorDeltaX = 0.0;
            cursorDeltaY = 0.0;
        }
    }

    void mouseButtonFunc(GLFWwindow* window, int button, int action, int mods)
    {
        Engine* gEngine = (Engine*)glfwGetWindowUserPointer(window);
        gEngine->processMouseButtonInput(button, action);
    }

    // TODO: Think more about whether we need this or not
    // This can happen whenever the mouse move, so multiple times per frame
    void cursorPosFunc(GLFWwindow* window, double cursorX, double cursorY)
    {
        Engine* gEngine = (Engine*)glfwGetWindowUserPointer(window);
        /* This will only be triggered when mouse cursor is within the application window */
        gEngine->updateMouseCursorPosition(cursorX, cursorY);
    }

    void mouseScrollFunc(GLFWwindow* window, double xOffset, double yOffset)
    {
        Engine* gEngine = (Engine*)glfwGetWindowUserPointer(window);
        gEngine->processMouseScroll(xOffset, yOffset);
    }

    void keyFunc(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods)
    {
        Engine* gEngine = (Engine*)glfwGetWindowUserPointer(window);
        gEngine->processKey(key, action);
    }

    void Engine::finalize()
    {
        m_graphicsSystem->getRenderer()->finalize();
    }

    void Engine::registerKeyCallback(KeyCallback* callback)
    {
        keyCallback = callback;
    }

    void Engine::registerMouseCursorCallback(MouseCursorCallback* callback)
    {
        mouseCursorCallback = callback;
    }

    void Engine::registerMouseButtonCallback(MouseButtonCallback* callback)
    {
        mouseButtonCallbacks.push_back(callback);
    }

    void Engine::registerMouseScrollWheelCallback(MouseScrollWheelCallback* callback)
    {
        mouseScrollCallback = callback;
    }

    void Engine::updateMouseCursorPosition(double x, double y)
    {
        // First time the cursor callback happens
        if (cursorX < 0.0 || cursorY < 0.0)
        {
            cursorX = x;
            cursorY = y;
        }

        cursorDeltaX = x - cursorX;
        cursorDeltaY = y - cursorY;
        cursorX = x;
        cursorY = y;

        // As of right now, only update camera rotation
        mouseCursorCallback(cursorX, cursorY, cursorDeltaX, cursorDeltaY);
    }

    // TODO: Defines CYAN_PRESS, CYAN_RELEASE as wrapper over glfw
    void Engine::processMouseButtonInput(int button, int action)
    {
        for (auto& callback: mouseButtonCallbacks)
        {
            callback(button, action);
        }
    }

    void Engine::processMouseScroll(double xOffset, double yOffset)
    {
        mouseScrollCallback(xOffset, yOffset);
    }

    void Engine::processKey(i32 key, i32 action)
    {
        keyCallback(key, action);
    }
}