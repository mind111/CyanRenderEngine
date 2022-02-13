#include "GraphicsSystem.h"

namespace Cyan
{
    GraphicsSystem* GraphicsSystem::m_singleton = 0;
    GraphicsSystem::GraphicsSystem(const glm::vec2& windowSize)
        : m_windowSize(windowSize)
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
        m_renderer = new Renderer;
        m_renderer->initialize(m_windowSize);
        
        // configure some necessary gl global states
        // this is necessary as we are setting z component of
        // the cubemap mesh to 1.f
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