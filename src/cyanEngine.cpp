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

CyanEngine::CyanEngine()
{
    
}

void mouseButtonFunc(GLFWwindow* window, int button, int action, int mods)
{
    CyanEngine* gEngine = (CyanEngine*)glfwGetWindowUserPointer(window);
    gEngine->processMouseButtonInput(button, action);
}

// TODO: Think more about whether we need this or not
// This can happen whenever the mouse move, so multiple times per frame
void cursorPosFunc(GLFWwindow* window, double xPos, double yPos)
{
    CyanEngine* gEngine = (CyanEngine*)glfwGetWindowUserPointer(window);
    /* This will only be triggered when mouse cursor is within the application window */
    gEngine->updateMouseCursorPosition(xPos, yPos);
}

void mouseScrollFunc(GLFWwindow* window, double xOffset, double yOffset)
{
    CyanEngine* gEngine = (CyanEngine*)glfwGetWindowUserPointer(window);
    gEngine->processMouseScroll(xOffset, yOffset);
}

void keyFunc(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods)
{
    CyanEngine* gEngine = (CyanEngine*)glfwGetWindowUserPointer(window);
    gEngine->processKey(key, action);
}

void CyanEngine::init(WindowConfig windowConfig, glm::vec2 sceneViewportPos, glm::vec2 renderSize)
{
    // window init
    if (!glfwInit())
    {
        std::cout << "err initializing glfw..! \n";
    }

    // Always on-top window
    // glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    m_window.mpWindow = glfwCreateWindow(windowConfig.width, windowConfig.height,
                                        "-- Cyan --", nullptr, nullptr);
    m_window.width = windowConfig.width;
    m_window.height = windowConfig.height;
    m_sceneViewportPostion = sceneViewportPos;
    m_renderSize = renderSize;

    glfwMakeContextCurrent(m_window.mpWindow);
    if (glewInit())
    {
        std::cout << "err initializing glew..! \n";
    }

    /* Setup input processing */
    glfwSetWindowUserPointer(m_window.mpWindow, this);
    glfwSetMouseButtonCallback(m_window.mpWindow, mouseButtonFunc);
    glfwSetCursorPosCallback(m_window.mpWindow, cursorPosFunc);
    glfwSetScrollCallback(m_window.mpWindow, mouseScrollFunc);
    glfwSetKeyCallback(m_window.mpWindow, keyFunc);

    // setup graphics system 
    {
        m_graphicsSystem = new Cyan::GraphicsSystem(glm::vec2(m_renderSize.x, m_renderSize.y));
        Cyan::getCurrentGfxCtx()->setWindow(&m_window);
    }
    // Init member variables
    {
        cursorX = -1.0;
        cursorY = -1.0;
        cursorDeltaX = 0.0;
        cursorDeltaY = 0.0;
    }

}

glm::vec2 CyanEngine::getSceneViewportPos()
{
    return m_sceneViewportPostion;
}

void CyanEngine::processInput()
{
    glfwPollEvents();
}

void CyanEngine::shutDown()
{

}

void CyanEngine::displaySliderFloat(const char* title, float* address, float min, float max)
{
    ImGui::SliderFloat(title, address, min, max, nullptr);
}

void CyanEngine::displayFloat3(const char* title, glm::vec3& v, bool isStatic)
{
    float buffer[3];

    buffer[0] = v.x;
    buffer[1] = v.y;
    buffer[2] = v.z;

    if (isStatic)
    {
        ImGui::InputFloat3(title, buffer, "%.3f", ImGuiInputTextFlags_ReadOnly);
    }
    else
    {
        ImGui::InputFloat3(title, buffer, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue);
        v.x = buffer[0];
        v.y = buffer[1];
        v.z = buffer[2];
    }
}

void CyanEngine::registerKeyCallback(KeyCallback* callback)
{
    keyCallback = callback;
}

void CyanEngine::registerMouseCursorCallback(MouseCursorCallback* callback)
{
    mouseCursorCallback = callback;
}

void CyanEngine::registerMouseButtonCallback(MouseButtonCallback* callback)
{
    mouseButtonCallbacks.push_back(callback);
}

void CyanEngine::registerMouseScrollWheelCallback(MouseScrollWheelCallback* callback)
{
    mouseScrollCallback = callback;
}

void CyanEngine::updateMouseCursorPosition(double x, double y)
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
void CyanEngine::processMouseButtonInput(int button, int action)
{
    for (auto& callback: mouseButtonCallbacks)
    {
        callback(button, action);
    }
}

void CyanEngine::processMouseScroll(double xOffset, double yOffset)
{
    mouseScrollCallback(xOffset, yOffset);
}

void CyanEngine::processKey(i32 key, i32 action)
{
    keyCallback(key, action);
}