#pragma once

#include <string>
#include <memory>

#include "Engine.h"

namespace Cyan
{
    class World;

    class ENGINE_API App
    {
    public:
        App(const char* name, int windowWidth, int windowHeight);
        ~App();

        void initalize();
        void deinitialize();
        void run();
        void update();
        void render();

    protected:
        virtual void customInitialize() { }
        virtual void customUpdate() { }
        virtual void customRender() { }
        virtual void customDeinitialize() { }

        std::string m_name;
        int m_windowWidth, m_windowHeight;
        bool bRunning = false;
        Engine* m_engine = nullptr;
        std::unique_ptr<World> m_world = nullptr;
    };
}
