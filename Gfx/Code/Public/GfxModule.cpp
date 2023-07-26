#include <chrono>
#include <thread>

#include "glfw3.h"
#include "GfxModule.h"
#include "SceneRender.h"
#include "SceneRenderer.h"
#include "GfxContext.h"

namespace Cyan
{
    GfxModule* GfxModule::s_instance = nullptr;
    GfxModule::GfxModule(const glm::uvec2& windowSize)
        : m_renderFrameNumber(0)
        , m_renderThread()
        , m_renderThreadID()
        , m_frameQueueMutex()
        , m_frameQueue()
        , m_windowSize(windowSize)
    {
        m_sceneRenderer = std::make_unique<SceneRenderer>();
    }

    GfxModule::~GfxModule()
    {

    }

    std::unique_ptr<GfxModule> GfxModule::create(const glm::uvec2& windowSize)
    {
        static std::unique_ptr<GfxModule> s_gfx(new GfxModule(windowSize));
        if (s_instance == nullptr)
        {
            s_instance = s_gfx.get();
        }
        return std::move(s_gfx);
    }

    GfxModule* GfxModule::get()
    {
        return s_instance;
    }

    void GfxModule::enqueueFrame(const Frame& frame)
    {
        std::lock_guard<std::mutex> lock(s_instance->m_frameQueueMutex);
        s_instance->m_frameQueue.push(frame);
    }

    bool GfxModule::tryPopOneFrame(Frame& outFrame)
    {
        std::lock_guard<std::mutex> lock(m_frameQueueMutex);
        bool bSuccess = false;
        if (!m_frameQueue.empty())
        {
            const Frame& frame = m_frameQueue.front();
            m_frameQueue.pop();
            outFrame = frame;
            bSuccess = true;
        }
        return bSuccess;
    }

    void GfxModule::beginFrame()
    {

    }

    void GfxModule::renderOneFrame()
    {
        Frame frame{ };
        if (tryPopOneFrame(frame))
        {
            beginFrame();

            // rendering a frame
            printf("[Render Thread] Rendering frame %d \n", frame.simFrameNumber);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            endFrame();
        }
        else
        {
            // wait for a new frame to arrive
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // todo: need a way to stop the render thread
    static void renderLoop()
    {
        // initialize gfx context

        GfxModule* gfxModule = GfxModule::get();
        if (gfxModule != nullptr)
        {
            static bool bRunning = true;
            while (bRunning)
            {
                gfxModule->renderOneFrame();
            }
        }
    }

    void GfxModule::initialize()
    {
#if !THREADED_RENDERING
        // create window
        if (glfwInit() != GLFW_TRUE)
        {
            assert(0);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
        m_glfwWindow = glfwCreateWindow(m_windowSize.x, m_windowSize.y, "Cyan", nullptr, nullptr);
#else
        // launch a thread to run renderLoop()
        m_renderThread = std::thread(renderLoop);
        m_renderThreadID = m_renderThread.get_id();
        m_renderThread.detach();
#endif
    }

    void GfxModule::deinitialize()
    {
    }

    bool GfxModule::isInRenderThread()
    {
        assert(s_instance != nullptr);
        std::thread::id currentThreadID = std::this_thread::get_id();
        return currentThreadID == s_instance->m_renderThreadID;
    }

    void GfxModule::endFrame()
    {
        m_renderFrameNumber.fetch_add(1);
    }

    SceneView::SceneView(const glm::uvec2& renderResolution)
    {
        m_render = std::make_unique<SceneRender>(renderResolution);

        m_state.resolution = renderResolution;
        m_state.aspectRatio = 1.f;
        m_state.viewMatrix = glm::mat4(1.f);
        m_state.prevFrameViewMatrix = glm::mat4(1.f);
        m_state.projectionMatrix = glm::mat4(1.f);
        m_state.prevFrameProjectionMatrix = glm::mat4(1.f);
        m_state.cameraPosition = glm::vec3(0.f);
        m_state.prevFrameCameraPosition = glm::vec3(0.f);
        m_state.cameraLookAt = glm::vec3(0.f);
        m_state.cameraRight = glm::vec3(0.f);
        m_state.cameraForward = glm::vec3(0.f);
        m_state.cameraUp = glm::vec3(0.f, 1.f, 0.f);
        m_state.frameCount = 0;
        m_state.elapsedTime = 0.f;
        m_state.deltaTime = 0.f;
    }

    SceneView::~SceneView()
    {
    }
}
