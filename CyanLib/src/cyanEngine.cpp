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
        m_IOSystem->update();
        m_graphicsSystem->setScene(m_scene);
        m_graphicsSystem->update();
    }

    void Engine::beginRender()
    {
        if (m_renderer)
        {
            m_renderer->beginRender();
        }
    }

    void Engine::render()
    {
        if (m_renderer)
        {
            m_renderer->render(m_scene.get());
        }
    }

    void Engine::endRender()
    {
        if (m_renderer)
        {
            m_renderer->endRender();
        }
    }

    void Engine::finalize()
    {
        m_IOSystem->finalize();
        m_graphicsSystem->finalize();
    }
}