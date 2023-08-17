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

    using PostRenderSceneViewsFunc = std::function<void(std::vector<SceneView*>*)>;
    using RenderToBackBufferFunc = std::function<void(struct Frame& frame)>;
    using UIRenderCommand = std::function<void(struct ImGuiContext*)>;

    enum class RenderingStage
    {
        kPreSceneRendering = 0,
        kPostSceneRendering,
        kPostRenderToBackBuffer,
        kCount
    };

    struct FrameGfxTask
    {
        std::string debugName;
        std::function<void(struct Frame& frame)> lambda;
    };

    struct Frame
    {
        void update();

        u32 simFrameNumber;
        Scene* scene = nullptr;
        std::vector<SceneView*>* views = nullptr;
        RenderingStage renderingStage = RenderingStage::kPreSceneRendering;
        std::queue<FrameGfxTask> gfxTaskQueues[(i32)RenderingStage::kCount];
        std::queue<UIRenderCommand> UICommands;
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
        static std::unique_ptr<GfxModule> create(const char* windowTitle, const glm::uvec2& windowSize);
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

        void renderOneFrame();
        void renderUI(Frame& frame);

        // provide ways to override some rendering behavior
        void setPostRenderSceneViews(const PostRenderSceneViewsFunc& postRenderSceneView);

        static PostRenderSceneViewsFunc s_defaultPostRenderSceneViews;
        static RenderToBackBufferFunc s_defaultRenderToBackBufferFunc;
    private:
        GfxModule(const char* windowTitle, const glm::uvec2& windowSize);

        void handleInputs();

        /* threaded rendering */
        std::atomic<i32> m_renderFrameNumber;
        std::thread m_renderThread;
        std::thread::id m_renderThreadID;
        std::mutex m_frameQueueMutex;
        std::queue<Frame> m_frameQueue;
        
        /* window related */
        const char* m_windowTitle = nullptr;
        glm::uvec2 m_windowSize;
        GLFWwindow* m_glfwWindow = nullptr;

        /* user input related */
        std::unique_ptr<InputEventHandler> m_inputHandler = nullptr;
        std::queue<std::unique_ptr<ILowLevelInputEvent>> m_inputEventQueue;

        /**/
        GfxContext* m_gfxCtx = nullptr;
        ShaderManager* m_shaderManager = nullptr;
        RenderingUtils* m_renderingUtils = nullptr;
        SceneRenderer* m_sceneRenderer = nullptr;

        PostRenderSceneViewsFunc m_postRenderSceneViews;
        RenderToBackBufferFunc m_renderToBackBuffer;
        std::queue<UIRenderCommand> m_UIRenderCommands;

        static GfxModule* s_instance;
    };
}
