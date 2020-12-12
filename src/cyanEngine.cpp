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
    ToolKit::loadScene(*scene, filename);
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
        mWindow.mpWindow = glfwCreateWindow(WindowConfig.width, WindowConfig.height,
                                            "-- Cyan --", nullptr, nullptr);
        glfwMakeContextCurrent(mWindow.mpWindow);
        if (glewInit())
        {
            std::cout << "err initializing glew..! \n";
        }

        /* Setup input processing */
        glfwSetWindowUserPointer(mWindow.mpWindow, this);
        glfwSetMouseButtonCallback(mWindow.mpWindow, mouseButtonFunc);
        glfwSetCursorPosCallback(mWindow.mpWindow, cursorPosFunc);
    }

    // Setup renderer 
    {
        mRenderer = CyanRenderer::get();
        mRenderer->bindSwapBufferCallback(std::bind(&CyanEngine::swapBuffers, this));
    }

    // Init member variables
    {
        cursorX = -1.0;
        cursorY = -1.0;
        cursorDeltaX = 0.0;
        cursorDeltaY = 0.0;
    }
}

void CyanEngine::render(Scene& scene)
{
    // Render a frame of current active scene
    mRenderer->render(scene, RenderConfig{ });
}

// TODO: Move glfwPollEvents() out of here
void CyanEngine::swapBuffers()
{
    glfwPollEvents();
    glfwSwapBuffers(mWindow.mpWindow);
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

// TODO: @camera orbit-mode controls
// TODO: @clean up control-flow with enabling MSAA
#if 0
void LegacyRender()
{
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    // |  Legacy code that needs to be refactored and rewrite  |
    // +++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    std::cout << "Hello Cyan!" << std::endl;
    if (!glfwInit()) {
        std::cout << "glfw init failed" << std::endl;
    }

    Window mainWindow = {
        nullptr, 0, 0, 0, 0,
        {0}
    };

    CyanEngine cyanEngine;

    windowManager.initWindow(mainWindow);
    glfwMakeContextCurrent(mainWindow.mpWindow);
    glewInit();

    // Initialization
    CyanRenderer* renderer = CyanRenderer::get();
    renderer->initRenderer();
    renderer->initShaders();

    Scene scene;
    sceneManager.loadSceneFromFile(scene, "scene/default_scene/scene_config.json");

    std::chrono::high_resolution_clock::time_point current, last = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> timeSpan; 

    while (!glfwWindowShouldClose(mainWindow.mpWindow)) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //----- FPS -----------------
        current = std::chrono::high_resolution_clock::now();
        timeSpan = std::chrono::duration_cast<std::chrono::duration<float>>(current - last);
        last = current;
        //---------------------------

        //---- Input handling -------
        handleInput(mainWindow, scene.mainCamera, .06); // TODO: need to swap out the hard-coded value later
        //---------------------------

        //-- framebuffer operations --
        glBindFramebuffer(GL_FRAMEBUFFER, CyanRenderer::get()->defaultFBO);

        if (renderer->enableMSAA) {
            renderer->setupMSAA();
        }
        //----------------------------

        //----- Draw calls -----------
        //----- Default first pass ---
        RenderConfig renderConfig = { };
        renderer->render(scene, renderConfig);
        //----------------------------

        if (renderer->enableMSAA) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, renderer->MSAAFBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderer->intermFBO);
            glBlitFramebuffer(0, 0, 800, 600, 0, 0, 800, 600, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        } else {

        }

        renderer->bloomPostProcess();
        renderer->blitBackbuffer();
    }
}
#endif
