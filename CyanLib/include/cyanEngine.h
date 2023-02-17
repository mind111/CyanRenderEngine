#pragma once 

#include "GraphicsSystem.h"
#include "IOSystem.h"
#include "Window.h"
#include "Singleton.h"

/** Cyan coding standard
* name of smart pointer objects should be suffixed with "Ptr" while raw pointer variables is not required to. It is assumed that raw pointers doesn't claim any sort of ownership
*/

class DefaultApp;

namespace Cyan
{

    class Engine : public Singleton<Engine>
    {
    public:
        Engine(u32 windowWidth, u32 windowHeight);
        ~Engine() { }

        GLFWwindow* getAppWindow() { return m_graphicsSystem->getAppWindow(); }
        IOSystem* getIOSystem() { return m_IOSystem.get(); }
        GraphicsSystem* getGraphicsSystem() { return m_graphicsSystem.get(); }

        f32 getFrameElapsedTimeInMs() { return renderFrameTime; }

        // main interface
        void initialize();
        void deinitialize();
        void update();

        using RenderFunc = std::function<void(GfxTexture2D* renderingOutput)>;
        void render(const RenderFunc& renderOneFrame);

        void render(Scene* scene, GfxTexture2D* sceneRenderingOutput, const std::function<void(Renderer*, GfxTexture2D*)>& postSceneRenderingCallbackonst = [](Renderer* renderer, GfxTexture2D* sceneRenderingOutput) {
            renderer->renderToScreen(sceneRenderingOutput);
        });

    private:
        // systems
        std::unique_ptr<IOSystem> m_IOSystem = nullptr;
        std::unique_ptr<GraphicsSystem> m_graphicsSystem = nullptr;
        // stats
        f32 renderFrameTime = 0.0f;
    };
}
