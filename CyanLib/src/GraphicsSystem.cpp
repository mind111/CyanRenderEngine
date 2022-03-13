#include "GraphicsSystem.h"

namespace Cyan
{
    GraphicsSystem* GraphicsSystem::m_singleton = 0;
    GraphicsSystem::GraphicsSystem(GLFWwindow* window, const glm::vec2& windowSize)
        : m_window(window), m_windowSize(windowSize)
    {
        if (!m_singleton)
            m_singleton = this;
        else
            CYAN_ASSERT(0, "Only one instance of GraphicsSystem is allowed!")

        initialize();
    }

    void GraphicsSystem::initialize()
    {
        m_sceneManager = new SceneManager;
        m_textureManager = new TextureManager;
        m_assetManager = new AssetManager;
        m_lightMapManager = new LightMapManager;
        m_pathTracer = new PathTracer;
        m_renderer = new Renderer(m_window, m_windowSize);
        
        // configure some necessary gl global states
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(4.f);
    }

    GraphicsSystem* GraphicsSystem::getSingletonPtr()
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