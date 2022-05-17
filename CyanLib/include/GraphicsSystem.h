#pragma once

#include "Scene.h"
#include "Texture.h"
#include "Asset.h"
#include "LightMap.h"
#include "CyanPathTracer.h"
#include "CyanRenderer.h"
#include "GfxContext.h"

namespace Cyan
{
    class GraphicsSystem
    {
    public:

        GraphicsSystem(u32 windowWidth, u32 windowHeight);
        ~GraphicsSystem() { }

        static GraphicsSystem* get();
        GLFWwindow* getAppWindow() { return m_glfwWindow; }
        Renderer* getRenderer();
        TextureManager* getTextureManager();
        AssetManager* getAssetManager();
        void initialize();

    private:
        static GraphicsSystem* m_singleton;
        SceneManager* m_sceneManager = nullptr;
        TextureManager* m_textureManager = nullptr;
        AssetManager* m_assetManager = nullptr;
        WindowManager* m_windowManager = nullptr;
        Renderer*     m_renderer = nullptr;
        // LightMapManager* m_lightMapManager;
        // PathTracer*      m_pathTracer;
        GLFWwindow* m_glfwWindow = nullptr;
        glm::uvec2 m_windowDimension; 
    };
}