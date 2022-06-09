#include <iostream>
#include <chrono>

#include "glew.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

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
    Engine* Engine::singleton = nullptr;

    Engine::Engine(u32 windowWidth, u32 windowHeight)
    {
        if (!singleton)
        {
            singleton = this;
            m_graphicsSystem = std::make_unique<GraphicsSystem>(windowWidth, windowHeight);
            m_IOSystem = std::make_unique<IOSystem>();
        }
        else
        {
            CYAN_ASSERT(0, "Attempted to instantiate multiple instances of Engine")
        }
    }

    void Engine::initialize()
    {
        m_graphicsSystem->initialize();
        m_IOSystem->initialize();

        m_renderer = m_graphicsSystem->getRenderer();
    }
    
    void Engine::update()
    {
        // todo: gather frame statistics

        // update window title
        static u32 numFrames = 0u;
        char windowTitle[64] = { };
        sprintf_s(windowTitle, "Cyan | Frame: %d | FPS: %.2f", numFrames++, 60.0f);
        glfwSetWindowTitle(m_graphicsSystem->getAppWindow(), windowTitle);

        m_IOSystem->update();
        m_graphicsSystem->setScene(m_scene);
        m_graphicsSystem->update();
    }

    void Engine::render()
    {
        if (m_renderer)
        {
            m_renderer->render(m_scene.get());
        }
    }

    void Engine::finalize()
    {
        m_IOSystem->finalize();
        m_graphicsSystem->finalize();
    }
}