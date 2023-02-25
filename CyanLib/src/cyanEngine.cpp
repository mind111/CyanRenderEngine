#include <iostream>
#include <chrono>

#include <glew/glew.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "Common.h"
#include "cyanRenderer.h"
#include "window.h"
#include "Shader.h"
#include "Scene.h"
#include "camera.h"
#include "mathUtils.h"
#include "cyanEngine.h"


namespace Cyan
{
    bool isMainThread()
    {
        return std::this_thread::get_id() == Engine::getMainThreadID();
    }

    Engine* Singleton<Engine>::singleton = nullptr;

    Engine::Engine(u32 windowWidth, u32 windowHeight)
    {
        // cache main thread id
        m_mainThreadID = std::this_thread::get_id();

        m_graphicsSystem = std::make_unique<GraphicsSystem>(windowWidth, windowHeight);
        m_IOSystem = std::make_unique<IOSystem>();
    }

    void Engine::initialize()
    {
        m_graphicsSystem->initialize();
        m_IOSystem->initialize();
    }
    
    void Engine::update() 
    {
        // todo: gather frame statistics

        // upload window title
        static u32 numFrames = 0u;
        char windowTitle[64] = { };
        sprintf_s(windowTitle, "Cyan | Frame: %d | FPS: %.2f", numFrames++, 60.0f);
        glfwSetWindowTitle(m_graphicsSystem->getAppWindow(), windowTitle);

        m_IOSystem->update();
        m_graphicsSystem->update();
    }

    void Engine::render(const RenderFunc& renderOneFrame)
    {
        if (m_graphicsSystem) 
        {
            ScopedTimer rendererTimer("Renderer Timer", false);
            m_graphicsSystem->render(renderOneFrame);
            rendererTimer.end();
            renderFrameTime = rendererTimer.m_durationInMs;
        }
    }

    void Engine::deinitialize()
    {
        m_IOSystem->deinitialize();
        m_graphicsSystem->deinitialize();
    }
}