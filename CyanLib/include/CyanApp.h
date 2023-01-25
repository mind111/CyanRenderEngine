#pragma once

#include "Common.h"
#include "CyanEngine.h" 
#include "Scene.h"

namespace Cyan
{
    class DefaultApp
    {
    public:
        DefaultApp(u32 appWindowWidth, u32 appWindowHeight);
        virtual ~DefaultApp() { }

        void initialize();
        void deinitialize();
        void update();
        void render();
        void run();

        virtual void customInitialize() { }
        virtual void customUpdate() { } 
        virtual void customRender(Renderer* renderer, Texture2D* sceneRenderingOutput) { }
        virtual void customFinalize() { }

    protected:
        glm::uvec2 m_appWindowDimension;
        bool isRunning = false;
        std::unique_ptr<Engine> gEngine;

        /** note: 
            allow creating multiple scenes, and scene instances are managed and tracked by SceneManager,
            app only holds reference to currently active scene.
        */ 
        std::shared_ptr<Scene> m_scene;
        Texture2D* m_sceneRenderingOutput = nullptr;
    };
}