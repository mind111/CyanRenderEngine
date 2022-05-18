#pragma once 

#include "imgui/imgui.h"

#include "GraphicsSystem.h"
#include "IOSystem.h"
#include "Window.h"

class DefaultApp;

namespace Cyan
{
    class Engine
    {
    public:
        Engine(u32 windowWidth, u32 windowHeight);
        ~Engine() { }

        static Engine* get() { return singleton; }
        GLFWwindow* getAppWindow() { return m_graphicsSystem->getAppWindow(); }
        IOSystem* getIOSystem() { return m_IOSystem.get(); }
        GraphicsSystem* getGraphicsSystem() { return m_graphicsSystem.get(); }

        // main interface
        void initialize();
        void finalize();
        void update();
        void render();

    private:
        static Engine* singleton;
        std::unique_ptr<IOSystem> m_IOSystem;
        std::unique_ptr<GraphicsSystem> m_graphicsSystem;
    };
}
