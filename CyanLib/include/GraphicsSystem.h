#pragma once

#include <memory>

#include "System.h"
#include "Scene.h"
#include "Texture.h"
#include "Asset.h"
#include "LightMap.h"
#include "CyanPathTracer.h"
#include "CyanRenderer.h"
#include "GfxContext.h"

namespace Cyan
{
    class GraphicsSystem : public System
    {
    public:

        GraphicsSystem(u32 windowWidth, u32 windowHeight);
        ~GraphicsSystem() { }

        virtual void initialize() override;
        virtual void finalize() override;
        virtual void update() override;

        static GraphicsSystem* get() { return singleton; }

        GLFWwindow* getAppWindow() { return m_glfwWindow; }
        glm::uvec2 getAppWindowDimension() { return m_windowDimension; }
        Renderer* getRenderer() { return m_renderer.get(); }
        SceneManager* getSceneManager() { return m_sceneManager.get(); }
        AssetManager* getAssetManager() { return m_assetManager.get(); }
        TextureManager* getTextureManager() { return m_textureManager.get(); }

    private:
        static GraphicsSystem* singleton;

        std::unique_ptr<SceneManager> m_sceneManager;
        std::unique_ptr<TextureManager> m_textureManager;
        std::unique_ptr<AssetManager> m_assetManager;
        std::unique_ptr<Renderer> m_renderer;
        // LightMapManager* m_lightMapManager;
        // PathTracer*      m_pathTracer;

        GLFWwindow* m_glfwWindow = nullptr;
        glm::uvec2 m_windowDimension; 
    };
}