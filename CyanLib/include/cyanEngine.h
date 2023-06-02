#pragma once 

#include <thread>

#include "GraphicsSystem.h"
#include "InputSystem.h"
#include "Window.h"
#include "Singleton.h"

class DefaultApp;

namespace Cyan
{
    bool isMainThread();

    class Engine : public Singleton<Engine>
    {
    public:
        Engine(u32 windowWidth, u32 windowHeight);
        ~Engine() { }

        static std::thread::id getMainThreadID() { assert(singleton); return singleton->m_mainThreadID; }

        GLFWwindow* getAppWindow() { return m_graphicsSystem->getAppWindow(); }
        InputSystem* getIOSystem() { return m_IOSystem.get(); }
        GraphicsSystem* getGraphicsSystem() { return m_graphicsSystem.get(); }

        f32 getFrameElapsedTimeInMs() { return renderFrameTime; }

        // main interface
        void initialize();
        void deinitialize();
        void update();

        using RenderFunc = std::function<void(GfxTexture2D* renderingOutput)>;
        void render(const RenderFunc& renderOneFrame);

    private:
        // main thread id
        std::thread::id m_mainThreadID;

        // systems
        std::unique_ptr<InputSystem> m_IOSystem = nullptr;
        std::unique_ptr<GraphicsSystem> m_graphicsSystem = nullptr;
        // stats
        f32 renderFrameTime = 0.0f;
    };
}
