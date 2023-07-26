#pragma once

#include <atomic>
#include <mutex>

#include "Core.h"
#include "Gfx.h"
#include "GfxInterface.h"

struct GLFWwindow;

namespace Cyan
{
#define THREADED_RENDERING 0

    class GfxContext;
    class Scene;
    class SceneCamera;
    class SceneRender;
    class SceneRenderer;

    class GFX_API SceneView
    {
    public:
        friend class Engine;

        // direct mirror of SceneCamera::RenderMode
        enum class ViewMode
        {
            kSceneColor = 0,
            kResolvedSceneColor,
            kSceneAlbedo,
            kSceneDepth,
            kSceneNormal,
            kSceneDirectLighting,
            kSceneIndirectLighting,
            kSceneLightingOnly,
            kSSGIAO,
            kSSGIDiffuse,
            kDebug,
            kWireframe,
            kCount
        };

        struct ViewSettings
        {

        };

        struct State
        {
            glm::uvec2 resolution;
            f32 aspectRatio;
            glm::mat4 viewMatrix;
            glm::mat4 prevFrameViewMatrix;
            glm::mat4 projectionMatrix;
            glm::mat4 prevFrameProjectionMatrix;
            glm::vec3 cameraPosition;
            glm::vec3 prevFrameCameraPosition;
            glm::vec3 cameraLookAt;
            glm::vec3 cameraRight;
            glm::vec3 cameraForward;
            glm::vec3 cameraUp;
            i32 frameCount;
            f32 elapsedTime;
            f32 deltaTime;
        };

        SceneView(const glm::uvec2& renderResolution);
        ~SceneView();

    private:
        std::unique_ptr<SceneRender> m_render = nullptr;
        State m_state;
    };

    using FrameGfxTask = std::function<void(struct Frame& frame)>;
    struct Frame
    {
        u32 simFrameNumber;
        std::queue<FrameGfxTask> gfxTasks;
        Scene* scene = nullptr;
        std::vector<SceneView*>* views = nullptr;
    };

    class RenderQueue
    {
    public:
    private:
    };

    /**
     * the Gfx module responsible for everything rendering related
     */
    class GFX_API GfxModule
    {
    public:
        friend class Engine;

        ~GfxModule();
        static std::unique_ptr<GfxModule> create(const glm::uvec2& windowSize);
        static GfxModule* get();
        static void enqueueFrame(const Frame& frame);
        bool tryPopOneFrame(Frame& outFrame);

        void initialize();
        void deinitialize();

        static bool isInRenderThread();

        void beginFrame();
        void endFrame();

        // non-threaded variant
        void renderOneFrame();
    private:
        GfxModule(const glm::uvec2& windowSize);

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

        /**/
        std::unique_ptr<GfxContext> m_gfxCtx = nullptr;
        std::unique_ptr<SceneRenderer> m_sceneRenderer = nullptr;

        static GfxModule* s_instance;
    };
}
