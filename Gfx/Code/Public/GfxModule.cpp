#include <chrono>
#include <thread>

#include "glew.h"
#include "glfw3.h"

#include "GfxModule.h"
#include "SceneRender.h"
#include "SceneRenderer.h"
#include "GfxContext.h"
#include "GHTexture.h"
#include "RenderingUtils.h"

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
        m_gfxCtx = GfxContext::get();
        m_shaderManager = ShaderManager::get();
        m_renderingUtils = RenderingUtils::get();
        m_sceneRenderer = std::make_unique<SceneRenderer>();
    }

    void GfxModule::handleInputs()
    {
        /**
         * This is kind of awkward that glfwPollEvents() has to be called on render thread where the gl context lives,
         * which means that the input handling functions are happening on render thread instead of main thread, where this kind of
         * things should live really.
         */
        glfwPollEvents();

        /**
         * Assuming input events are all gathered past the call to glfwPollEvents(), they are ready to be shipped to the main thread 
         */
        if (m_inputHandler != nullptr && !m_inputEventQueue.empty())
        {
            m_inputHandler->handle(m_inputEventQueue);
        }
        /**
         * Either there is no events this frame or all the events are queued up on the main thread past this point
         */
        assert(m_inputEventQueue.empty());
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
            Frame& frame = m_frameQueue.front();
            outFrame = frame;
            m_frameQueue.pop();
            bSuccess = true;
        }
        return bSuccess;
    }

    void GfxModule::beginFrame()
    {
    }

    void GfxModule::renderOneFrame()
    {
        beginFrame();
        {
            Frame frame{ };
            if (tryPopOneFrame(frame))
            {
                handleInputs();

                // rendering a frame
                // cyanInfo("Rendering frame %d", frame.simFrameNumber);

                // execute frame gfx tasks first
                frame.flushGfxTasks();

                if (frame.scene != nullptr && frame.views != nullptr)
                {
                    // render views
                    auto& views = *(frame.views);
                    for (auto view : views)
                    {
                        m_sceneRenderer->render(frame.scene, *view);
                    }

                    // todo: for now, assuming that views[0] is always the main view, need to think about a proper way to 
                    // abstract this
                    // blit views[0]'s result to the default backbuffer
                    SceneView* defaultView = views[0];
                    GHTexture2D* output = defaultView->getOutput();
                    RenderingUtils::renderToBackBuffer(output);

                    // only increment frame counter when actual rendering work happened
                    m_renderFrameNumber.fetch_add(1);
                }
            }
            else
            {
                // wait for a new frame to arrive
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        // we always swap buffer even there is no work done for this frame
        endFrame();
    }

    static void glErrorCallback(GLenum inSource, GLenum inType, GLuint inID, GLenum inSeverity, GLsizei inLength, const GLchar* inMessage, const void* userParam) 
    {
        std::string source, type, severity;
        switch (inSource) 
        {
        case GL_DEBUG_SOURCE_API:
            source = "API";
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            source = "WINDOW SYSTEM";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            source = "SHADER COMPILER";
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            source = "THIRD PARTY";
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            source = "APPLICATION";
            break;
        case GL_DEBUG_SOURCE_OTHER:
        default:
            source = "UNKNOWN";
            break;
        }
        switch (inType) 
        {
        case GL_DEBUG_TYPE_ERROR:
            type = "ERROR";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            type = "DEPRECATED BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            type = "UDEFINED BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            type = "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            type = "PERFORMANCE";
            break;
        case GL_DEBUG_TYPE_OTHER:
            type = "OTHER";
            break;
        case GL_DEBUG_TYPE_MARKER:
            type = "MARKER";
            break;
        default:
            type = "UNKNOWN";
            break;
        }
        switch (inSeverity) {
        case GL_DEBUG_SEVERITY_HIGH:
            severity = "HIGH";
            break;
            severity = "MEDIUM";
            break;
        case GL_DEBUG_SEVERITY_LOW:
            severity = "LOW";
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            severity = "NOTIFICATION";
            break;
        default:
            severity = "UNKNOWN";
            break;
        }

        switch (inType) 
        {
        case GL_DEBUG_TYPE_ERROR:
            type = "ERROR";
            cyanError("Source: %s, Type: %s, Severity: %s, Message: %s", source.c_str(), type.c_str(), severity.c_str(), inMessage);
            __debugbreak();
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            type = "DEPRECATED BEHAVIOR";
            cyanError("Source: %s, Type: %s, Severity: %s, Message: %s", source.c_str(), type.c_str(), severity.c_str(), inMessage);
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            type = "UNDEFINED BEHAVIOR";
            cyanError("Source: %s, Type: %s, Severity: %s, Message: %s", source.c_str(), type.c_str(), severity.c_str(), inMessage);
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            type = "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            type = "PERFORMANCE";
            break;
        case GL_DEBUG_TYPE_OTHER:
            type = "OTHER";
            break;
        case GL_DEBUG_TYPE_MARKER:
            type = "MARKER";
            break;
        default:
            type = "UNKNOWN";
            break;
        }
    }

    static char translateKey(i32 key)
    {
        switch (key)
        {
        case GLFW_KEY_W: return 'W';
        case GLFW_KEY_A: return 'A';
        case GLFW_KEY_S: return 'S';
        case GLFW_KEY_D: return 'D';
        default:
            return '\0';
        }
    } 

    static InputAction translateAction(i32 action)
    {
        switch (action)
        {
        case GLFW_PRESS: return InputAction::kPress;
        case GLFW_RELEASE: return InputAction::kRelease;
        case GLFW_REPEAT: return InputAction::kRepeat;
        default: assert(0); return InputAction::kCount;
        }
    }

    static void onKeyEvent(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods)
    {
        auto keyEvent = std::make_unique<LowLevelKeyEvent>(translateKey(key), translateAction(action));
        auto gfxModule = GfxModule::get();
        assert(gfxModule != nullptr);
        gfxModule->enqueueInputEvent(std::move(keyEvent));
    }

    static void onMouseCursorEvent(GLFWwindow* window, f64 x, f64 y)
    {
        static glm::dvec2 currrentCursorPosition(x, y);
        glm::dvec2 delta = glm::dvec2(x, y) - currrentCursorPosition;
        currrentCursorPosition = glm::dvec2(x, y);
        auto mouseCursorEvent = std::make_unique<LowLevelMouseCursorEvent>(currrentCursorPosition, delta);
        auto gfxModule = GfxModule::get();
        assert(gfxModule != nullptr);
        gfxModule->enqueueInputEvent(std::move(mouseCursorEvent));
    }

    static void onMouseButtonEvent(GLFWwindow* window, i32 button, i32 action, i32 mods)
    {
        MouseButton outButton;
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT: outButton = MouseButton::kMouseLeft; break;
        case GLFW_MOUSE_BUTTON_RIGHT: outButton = MouseButton::kMouseRight; break;
        default: break;
        }
        auto mouseButtonEvent = std::make_unique<LowLevelMouseButtonEvent>(outButton, translateAction(action));
        auto gfxModule = GfxModule::get();
        assert(gfxModule != nullptr);
        gfxModule->enqueueInputEvent(std::move(mouseButtonEvent));
    }

    static void onMouseWheelEvent(GLFWwindow* window, f64 xOffset, f64 yOffset)
    {
    }

    // todo: need a way to stop the render thread
    static void renderLoop()
    {
        GfxModule* gfxModule = GfxModule::get();
        if (gfxModule != nullptr)
        {
            // todo: this should be moved into GLContext::init()
            // init gl
            {
                if (glfwInit() != GLFW_TRUE)
                {
                    assert(0);
                }
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
                glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
                gfxModule->m_glfwWindow = glfwCreateWindow(gfxModule->m_windowSize.x, gfxModule->m_windowSize.y, "Cyan", nullptr, nullptr);
                glfwMakeContextCurrent(gfxModule->m_glfwWindow);
                GLenum glewErr = glewInit();
                assert(glewErr == GLEW_OK);

                // configure some gl global states
                glDepthFunc(GL_LEQUAL);
                glEnable(GL_LINE_SMOOTH);
                glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
                glEnable(GL_CULL_FACE);
                glFrontFace(GL_CCW);
                glCullFace(GL_BACK);
                glLineWidth(6.f);
                glfwSwapInterval(0);
                glEnable(GL_PROGRAM_POINT_SIZE);

                // setup error callback
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                glDebugMessageCallback(glErrorCallback, nullptr);

                // bind I/O handlers 
                glfwSetWindowUserPointer(gfxModule->m_glfwWindow, gfxModule);
                glfwSetMouseButtonCallback(gfxModule->m_glfwWindow, onMouseButtonEvent);
                glfwSetCursorPosCallback(gfxModule->m_glfwWindow, onMouseCursorEvent);
                glfwSetScrollCallback(gfxModule->m_glfwWindow, onMouseWheelEvent);
                glfwSetKeyCallback(gfxModule->m_glfwWindow, onKeyEvent);
            }

            gfxModule->m_renderingUtils->initialize();

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
        glfwMakeContextCurrent(m_glfwWindow);
        GLenum glewErr = glewInit();
        assert(glewErr == GLEW_OK);

        // configure some gl global states
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
        glLineWidth(6.f);
        glfwSwapInterval(0);
        glEnable(GL_PROGRAM_POINT_SIZE);
        
        // setup error callback
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glErrorCallback, nullptr);
        m_renderThreadID = std::this_thread::get_id();
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

    void GfxModule::setInputEventHandler(std::unique_ptr<InputEventHandler> eventHandler)
    {
        m_inputHandler = std::move(eventHandler);
    }

    void GfxModule::enqueueInputEvent(std::unique_ptr<ILowLevelInputEvent> inputEvent)
    {
        m_inputEventQueue.push(std::move(inputEvent));
    }

    bool GfxModule::isInRenderThread()
    {
        assert(s_instance != nullptr);
        std::thread::id currentThreadID = std::this_thread::get_id();
        return currentThreadID == s_instance->m_renderThreadID;
    }

    void GfxModule::endFrame()
    {
        glfwSwapBuffers(m_glfwWindow);
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

    GHTexture2D* SceneView::getOutput()
    {
        switch (m_viewMode)
        {
        case ViewMode::kSceneColor: return m_render->color();
        case ViewMode::kResolvedSceneColor: return m_render->resolvedColor();
        case ViewMode::kSceneAlbedo: return m_render->albedo();
        case ViewMode::kSceneDepth: return m_render->depth();
        case ViewMode::kSceneNormal: return m_render->normal();
        case ViewMode::kSceneDirectLighting: return m_render->directLighting();
        case ViewMode::kSceneIndirectLighting: return m_render->indirectLighting();
        default:
            assert(0);
            return nullptr;
        }
    }

    void Frame::flushGfxTasks()
    {
        assert(GfxModule::isInRenderThread());
        while (!gfxTasks.empty())
        {
            auto& task = gfxTasks.front();
            // cyanInfo("Executing Gfx task %s", task.debugName.c_str());
            task.lambda(*this);
            gfxTasks.pop();
        }
    }
}
