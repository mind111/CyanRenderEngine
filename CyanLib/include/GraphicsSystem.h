#pragma once

#include <memory>

#include "System.h"
#include "Scene.h"
#include "Texture.h"
#include "AssetManager.h"
#include "LightMap.h"
#include "CyanPathTracer.h"
#include "CyanRenderer.h"
#include "GfxContext.h"

namespace Cyan
{
    // todo: implement WindowManager to manage window related operations, and GraphicsSystem should be the owner of the 
    // WindowManager
    class GraphicsSystem : public System, public Singleton<GraphicsSystem>
    {
    public:

        GraphicsSystem(u32 windowWidth, u32 windowHeight);
        ~GraphicsSystem() { }

        virtual void initialize() override;
        virtual void finalize() override;
        virtual void update() override;

        GLFWwindow* getAppWindow() { return m_glfwWindow; }
        glm::uvec2 getAppWindowDimension() { return m_windowDimension; }
        Renderer* getRenderer() { return m_renderer.get(); }
        SceneManager* getSceneManager() { return m_sceneManager.get(); }
        AssetManager* getAssetManager() { return m_assetManager.get(); }
        // TextureManager* getTextureManager() { return m_textureManager.get(); }

        void setScene(std::shared_ptr<Scene> scene) { m_scene = scene; }

    private:
        std::unique_ptr<SceneManager> m_sceneManager;
        std::unique_ptr<AssetManager> m_assetManager;
        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<ShaderManager> m_shaderManager;
        // LightMapManager* m_lightMapManager;
        // PathTracer*      m_pathTracer;

        std::unique_ptr<GfxContext> m_ctx;
        std::shared_ptr<Scene> m_scene;
        GLFWwindow* m_glfwWindow = nullptr;
        glm::uvec2 m_windowDimension; 
    };
}