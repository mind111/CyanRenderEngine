#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "CyanEngine.h"
#include "GraphicsSystem.h"
#include "AssetManager.h"
#include "AssetImporter.h"

// A hack to force application uses Nvidia discrete gpu, I'm actually not sure if
// this is doing anything at all
#ifdef __cplusplus
extern "C" {
#endif
    _declspec(dllexport) u32 NvOptimusEnablement = 0x00000001;
#ifdef __cplusplus
}
#endif

namespace Cyan
{
    GraphicsSystem* Singleton<GraphicsSystem>::singleton = nullptr;

    // error callback
    typedef void (APIENTRY* DEBUGPROC)(GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* userParam);

    void glErrorCallback(GLenum inSource, GLenum inType, GLuint inID, GLenum inSeverity, GLsizei inLength, const GLchar* inMessage, const void* userParam) 
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

    GraphicsSystem::GraphicsSystem(u32 windowWidth, u32 windowHeight)
        : m_windowDimension({ windowWidth, windowHeight })
    {
        // create window
        if (glfwInit() != GLFW_TRUE)
        {
            cyanError("Error initializing glfw!")
            assert(0);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
        m_glfwWindow = glfwCreateWindow(m_windowDimension.x, m_windowDimension.y, "Cyan", nullptr, nullptr);
        assert(m_glfwWindow);

        glfwMakeContextCurrent(m_glfwWindow);
        GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK)
        {
            cyanError("Error initializing glew: %s!", glewGetErrorString(glewErr));
            assert(0);
        }

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

        m_ctx = std::make_unique<GfxContext>(m_glfwWindow);
        // todo: refactor asset related stuffs into something like AssetSystem
        m_assetManager = std::make_unique<AssetManager>();
        m_shaderManager = std::make_unique<ShaderManager>();
        m_renderer = std::make_unique<Renderer>(m_ctx.get(), windowWidth, windowHeight);
        m_gfxCommandQueue = std::make_unique<GfxCommandQueue>();

        GfxTexture2D::Spec spec(m_windowDimension.x, m_windowDimension.y, 1, PF_RGB16F);
        Sampler2D sampler;
        m_renderingOutput = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, sampler));
    } 

    GraphicsSystem::~GraphicsSystem() { }

    void GraphicsSystem::initialize()
    {
        // todo: specify dependencies between modules, for example, ShaderManager needs to be initialized before AssetManager
        m_ctx->initialize();
        m_shaderManager->initialize();
        m_assetManager->initialize();
        m_renderer->initialize();

        // ui
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();

            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

            ImGui::StyleColorsDark();

            ImGui_ImplGlfw_InitForOpenGL(m_glfwWindow, false);
            ImGui_ImplOpenGL3_Init();

            // font
            static ImFont* gFont = nullptr;
            gFont = io.Fonts->AddFontFromFileTTF("C:\\dev\\cyanRenderEngine\\asset\\fonts\\Roboto-Medium.ttf", 20.f);
        }
    }

    void GraphicsSystem::deinitialize()
    {

    }

    void GraphicsSystem::GfxCommandQueue::enqueueCommand(const GfxCommand& command)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(command);
    }

    void GraphicsSystem::GfxCommandQueue::executeCommands()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // todo: not draining (load balancing) the command queue to prevent stalling the rendering
        while (!m_queue.empty())
        {
            auto command = m_queue.front();
            m_queue.pop();
            command.exec();
        }
    }

    void GraphicsSystem::enqueueGfxCommand(const std::function<void()>& lambda)
    {
        GfxCommand command(lambda);
        if (isMainThread())
        {
            command.exec();
        }
        else
        {
            singleton->m_gfxCommandQueue->enqueueCommand(command);
        }
    }

    void GraphicsSystem::update() 
    {
        m_gfxCommandQueue->executeCommands();
    }

    void GraphicsSystem::render(const std::function<void(GfxTexture2D*)>& renderOneFrame) 
    {
        // clear default render target
        m_ctx->setFramebuffer(nullptr);
        m_ctx->clear();
        
        renderOneFrame(m_renderingOutput.get());

        m_ctx->present();
    }
}