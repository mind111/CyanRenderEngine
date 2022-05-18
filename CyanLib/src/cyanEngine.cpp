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
        // todo: this function should instead call System::initialize() in order to initialize subsystems
        m_graphicsSystem->initialize();
        m_IOSystem->initialize();

        // setup graphics system 
        {
            // Cyan::init();
            // m_graphicsSystem = new Cyan::GraphicsSystem(m_glfwWindow.mpWindow, glm::vec2(m_renderSize.x, m_renderSize.y));
            // Cyan::getCurrentGfxCtx()->setWindow(&m_glfwWindow);
        }
    }
    
    void Engine::update()
    {
        m_IOSystem->update();
    }

    void Engine::render()
    {

    }

    void Engine::finalize()
    {
        m_IOSystem->finalize();
        m_graphicsSystem->finalize();
    }
}