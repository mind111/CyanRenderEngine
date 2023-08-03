#pragma once

#include <atomic>
#include <mutex>

#include "Core.h"
#include "Gfx.h"
#include "LowLeveInputs.h"
#include "SceneView.h"

struct GLFWwindow;

namespace Cyan
{
#define THREADED_RENDERING 1

    class GfxContext;
    class Scene;
    class SceneCamera;
    class SceneRender;
    class RenderingUtils;
    class SceneRenderer;
    class ShaderManager;
    class GHTexture2D;

    struct FrameGfxTask
    {
        std::string debugName;
        std::function<void(struct Frame& frame)> lambda;
    };

    struct Frame
    {
        void flushGfxTasks();

        u32 simFrameNumber;
        std::queue<FrameGfxTask> gfxTasks;
        Scene* scene = nullptr;
        std::vector<SceneView*>* views = nullptr;
    };

    /** 
     * the Gfx module responsible for everything rendering related
     */
    class GFX_API GfxModule
    {
    public:
        friend class Engine;
        friend static void renderLoop();

        ~GfxModule();
        static std::unique_ptr<GfxModule> create(const glm::uvec2& windowSize);
        static GfxModule* get();
        static void enqueueFrame(const Frame& frame);
        bool tryPopOneFrame(Frame& outFrame);

        void initialize();
        void deinitialize();
        void setInputEventHandler(std::unique_ptr<InputEventHandler> eventHandler);
        void enqueueInputEvent(std::unique_ptr<ILowLevelInputEvent> inputEvent);

        glm::uvec2 getWindowSize() { return m_windowSize; }

        static bool isInRenderThread();

        void beginFrame();
        void endFrame();

        // non-threaded variant
        void renderOneFrame();
    private:
        GfxModule(const glm::uvec2& windowSize);

        void handleInputs();

        /* threaded rendering */
        std::atomic<i32> m_renderFrameNumber;
        std::thread m_renderThread;
        std::thread::id m_renderThreadID;
        std::mutex m_frameQueueMutex;
        std::queue<Frame> m_frameQueue;
        
        /* window related */
        glm::uvec2 m_windowSize;
        const char* m_windowTitle = nullptr;
        GLFWwindow* m_glfwWindow = nullptr;

        /* user input related */
        std::unique_ptr<InputEventHandler> m_inputHandler = nullptr;
        std::queue<std::unique_ptr<ILowLevelInputEvent>> m_inputEventQueue;

        /**/
        GfxContext* m_gfxCtx = nullptr;
        ShaderManager* m_shaderManager = nullptr;
        RenderingUtils* m_renderingUtils = nullptr;
        SceneRenderer* m_sceneRenderer = nullptr;

        static GfxModule* s_instance;
    };
}
