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

        /**
         * Allow derived application to overwrite the custom frame rendering function in here
         */
        virtual void customInitialize() { }
        virtual void customUpdate() { } 
        virtual void customFinalize() { }

    protected:
        glm::uvec2 m_appWindowDimension;
        bool isRunning = false;
        std::unique_ptr<Engine> m_engine = nullptr;
        Engine::RenderFunc m_renderOneFrame;
        std::shared_ptr<World> m_world = nullptr;
    };
}