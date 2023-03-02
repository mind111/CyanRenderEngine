#pragma once

#include <memory>

#include "System.h"
#include "Scene.h"
#include "Texture.h"
#include "LightMap.h"
#include "CyanRenderer.h"
#include "GfxContext.h"

namespace Cyan
{
    class AssetManager;
    class AssetImporter;

    // todo: implement WindowManager to manage window related operations, and GraphicsSystem should be the owner of the 
    // WindowManager
    class GraphicsSystem : public Singleton<GraphicsSystem>, public System
    {
    public:
        GraphicsSystem(u32 windowWidth, u32 windowHeight);
        ~GraphicsSystem();

        virtual void initialize() override;
        virtual void deinitialize() override;
        void update();
        void render(const std::function<void(GfxTexture2D* renderingOutput)>& renderOneFrame);

        GLFWwindow* getAppWindow() { return m_glfwWindow; }
        glm::uvec2 getAppWindowDimension() { return m_windowDimension; }
        Renderer* getRenderer() { return m_renderer.get(); }
        AssetManager* getAssetManager() { return m_assetManager.get(); }

        struct Settings 
        {
            bool bSuperSampling = true;
        } m_settings;

    private:
        std::unique_ptr<AssetManager> m_assetManager = nullptr;
        std::unique_ptr<AssetImporter> m_assetImporter = nullptr;
        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<ShaderManager> m_shaderManager;

        std::unique_ptr<GfxContext> m_ctx;
        GLFWwindow* m_glfwWindow = nullptr;
        glm::uvec2 m_windowDimension; 

        // rendering output 
        std::unique_ptr<GfxTexture2D> m_renderingOutput = nullptr;
    };
}