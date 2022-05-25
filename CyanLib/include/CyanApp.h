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
        void finalize();
        void update();
        void render();
        void run();

        virtual void customInitialize() = 0;
        virtual void customUpdate() = 0;
        virtual void customRender() = 0;
        virtual void customFinalize() = 0;

    protected:
        bool isRunning = false;
        std::unique_ptr<Engine> gEngine;

        //  allow creating multiple scenes, and scene instances are managed by engine, app only holds reference to currently active scene
        std::shared_ptr<Scene> m_scene;
    };
}