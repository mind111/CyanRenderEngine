#include "GraphicsSystem.h"
#include "CyanUI.h"

namespace Cyan
{
    GraphicsSystem* GraphicsSystem::m_singleton = 0;

    GraphicsSystem::GraphicsSystem(u32 windowWidth, u32 windowHeight)
        : m_windowDimension({ windowWidth, windowHeight })
    {
        if (!m_singleton)
        {
            m_singleton = this;
            m_sceneManager = new SceneManager;
            m_textureManager = new TextureManager;
            m_assetManager = new AssetManager;
            m_renderer = new Renderer(windowWidth, windowHeight);
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

    GraphicsSystem* GraphicsSystem::get()
    {
        return m_singleton;
    }

    Renderer* GraphicsSystem::getRenderer()
    {
        return m_renderer;
    }

    TextureManager* GraphicsSystem::getTextureManager()
    {
        return m_textureManager;
    }

    AssetManager* GraphicsSystem::getAssetManager()
    {
        return m_assetManager;
    }
}