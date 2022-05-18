#include "GraphicsSystem.h"
#include "CyanUI.h"

namespace Cyan
{
    GraphicsSystem* GraphicsSystem::singleton = 0;

    GraphicsSystem::GraphicsSystem(u32 windowWidth, u32 windowHeight)
        : m_windowDimension({ windowWidth, windowHeight })
    {
        if (!singleton)
        {
            singleton = this;
            m_sceneManager = std::make_unique<SceneManager>();
            m_textureManager = std::make_unique<TextureManager>();
            m_assetManager = std::make_unique<AssetManager>();
            m_renderer = std::make_unique<Renderer>(windowWidth, windowHeight);
            // m_lightMapManager = new LightMapManager;
            // m_pathTracer = new PathTracer;
        }
        else
        {
            CYAN_ASSERT(0, "Only one instance of GraphicsSystem is allowed!")
        }
    }

    void GraphicsSystem::initialize()
    {
        // create window
        if (!glfwInit())
        {
            cyanError("Error initializing glfw")
        }
        m_glfwWindow = glfwCreateWindow(m_windowDimension.x, m_windowDimension.y, "CyanEngine", nullptr, nullptr);
        glfwMakeContextCurrent(m_glfwWindow);

        // configure gl context
        if (glewInit())
        {
            cyanError("Error initializing glew")
        }
        // configure some gl global states
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(4.f);

        // ui
        UI::initialize(m_glfwWindow);
    }

    void GraphicsSystem::finalize()
    {

    }

    void GraphicsSystem::update()
    {

    }
}