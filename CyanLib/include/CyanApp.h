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


        virtual void initialize(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize) = 0;

        virtual void run() = 0;
        virtual void render() = 0;
        virtual void shutDown() = 0;

    protected:
        bool isRunning = false;
        Engine* gEngine = nullptr;
        Scene* m_scene = nullptr;
    };
}