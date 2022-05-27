#pragma once 

#include "imgui/imgui.h"

#include "GraphicsSystem.h"
#include "IOSystem.h"
#include "Window.h"

/** Cyan coding standard
* name of smart pointer objects should be suffixed with "Ptr" while raw pointer variables is not required to. It is assumed that raw pointers doesn't claim any sort of ownership
*/

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

        void setScene(std::shared_ptr<Scene> scene) { m_scene = scene; }

        // main interface
        void initialize();
        void finalize();
        void update();
        void render();
        void beginRender();
        void endRender();

    private:
        static Engine* singleton;

        // systems
        std::unique_ptr<IOSystem> m_IOSystem;
        std::unique_ptr<GraphicsSystem> m_graphicsSystem;

        // active scene
        std::shared_ptr<Scene> m_scene;
        Renderer* m_renderer = nullptr;
    };
}
