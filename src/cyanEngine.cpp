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

Scene* CyanEngine::loadScene(const char* filename)
{
    Scene* scene = new Scene();
    CyanRenderer::Toolkit::loadScene(*scene, filename);
    return scene;
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

void CyanEngine::setup(WindowConfig WindowConfig, const char* sceneFolderPath)
{
    // Setup window
    {
        if (!glfwInit())
        {
            std::cout << "err initializing glfw..! \n";
        }

        // Always on-top window
        // glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
        m_window.mpWindow = glfwCreateWindow(WindowConfig.width, WindowConfig.height,
                                            "-- Cyan --", nullptr, nullptr);
        m_window.width = WindowConfig.width;
        m_window.height = WindowConfig.height;

        glfwMakeContextCurrent(m_window.mpWindow);
        if (glewInit())
        {
            std::cout << "err initializing glew..! \n";
        }

        /* Setup input processing */
        glfwSetWindowUserPointer(m_window.mpWindow, this);
        glfwSetMouseButtonCallback(m_window.mpWindow, mouseButtonFunc);
        glfwSetCursorPosCallback(m_window.mpWindow, cursorPosFunc);
    }

    // Setup renderer 
    {
        m_renderer = CyanRenderer::get();
        m_renderer->bindSwapBufferCallback(std::bind(&CyanEngine::swapBuffers, this));
    }

    // Init member variables
    {
        cursorX = -1.0;
        cursorY = -1.0;
        cursorDeltaX = 0.0;
        cursorDeltaY = 0.0;
    }

    // Init & configure ImGui
    {
        /* Setup ImGui */
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window.mpWindow, true);
        ImGui_ImplOpenGL3_Init(nullptr);

        /* ImGui style */
        ImGuiStyle& style = ImGui::GetStyle();
        style.ChildRounding = 3.f;
        style.GrabRounding = 0.f;
        style.WindowRounding = 0.f;
        style.ScrollbarRounding = 3.f;
        style.FrameRounding = 3.f;
        style.WindowTitleAlign = ImVec2(0.5f,0.5f);

        style.Colors[ImGuiCol_Text]                  = ImVec4(0.73f, 0.73f, 0.73f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.26f, 0.26f, 0.26f, 0.95f);
        style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
        style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_Border]                = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
        style.Colors[ImGuiCol_Button]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
        style.Colors[ImGuiCol_Header]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
        style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.32f, 0.52f, 0.65f, 1.00f);
        style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    }
}

void CyanEngine::render(Scene& scene)
{
    // Render a frame of current active scene
    m_renderer->render(scene, RenderConfig{ });
}

// TODO: Move glfwPollEvents() out of here
void CyanEngine::swapBuffers()
{
    glfwPollEvents();
    glfwSwapBuffers(m_window.mpWindow);
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

void CyanEngine::registerMouseCursorCallback(MouseCursorCallback* callback)
{
    mouseCursorCallback = callback;
}

void CyanEngine::registerMouseButtonCallback(MouseButtonCallback* callback)
{
    mouseButtonCallbacks.push_back(callback);
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
    mouseCursorCallback(cursorDeltaX, cursorDeltaY);
}

// TODO: Defines CYAN_PRESS, CYAN_RELEASE as wrapper over glfw
void CyanEngine::processMouseButtonInput(int button, int action)
{
    for (auto& callback: mouseButtonCallbacks)
    {
        callback(button, action);
    }
}