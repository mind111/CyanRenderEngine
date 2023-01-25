#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "GraphicsSystem.h"

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
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            type = "DEPRECATED BEHAVIOR";
            cyanError("Source: %s, Type: %s, Severity: %s, Message: %s", source.c_str(), type.c_str(), severity.c_str(), inMessage);
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            type = "UDEFINED BEHAVIOR";
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
        : Singleton<GraphicsSystem>(),
        m_windowDimension({ windowWidth, windowHeight })
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
        if (!m_glfwWindow) {
            cyanError("Error creating a window!")
            assert(0);
        }
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
        m_assetManager = std::make_unique<AssetManager>();
        m_shaderManager = std::make_unique<ShaderManager>();
        m_renderer = std::make_unique<Renderer>(m_ctx.get(), windowWidth, windowHeight);
        // m_lightMapManager = new LightMapManager;
        // m_pathTracer = new PathTracer;
    }

    void GraphicsSystem::initialize()
    {
        // todo: specify dependencies between modules, for example, ShaderManager needs to be initialized before AssetManager
        // initialize managers
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
            ImGui::GetStyle().WindowRounding = 0.0f;
            ImGui::GetStyle().ChildRounding = 0.0f;
            ImGui::GetStyle().FrameRounding = 0.0f;
            ImGui::GetStyle().GrabRounding = 0.0f;
            ImGui::GetStyle().PopupRounding = 0.0f;
            ImGui::GetStyle().ScrollbarRounding = 0.0f;

            ImGui_ImplGlfw_InitForOpenGL(m_glfwWindow, false);
            ImGui_ImplOpenGL3_Init(nullptr);

            // font
            static ImFont* gFont = nullptr;
            gFont = io.Fonts->AddFontFromFileTTF("C:\\dev\\cyanRenderEngine\\asset\\fonts\\Roboto-Medium.ttf", 14.f);
        }
    }

    void GraphicsSystem::deinitialize()
    {

    }

    void GraphicsSystem::update() 
    {

    }

    void GraphicsSystem::render(Scene* scene, Texture2D* sceneRenderingOutput, const std::function<void(Renderer*, Texture2D*)>& postSceneRenderingCallback)
    {
        // clear default render target
        m_ctx->setRenderTarget(nullptr, { });
        m_ctx->clear();
        // scene rendering
        if (scene) 
        {
            if (auto camera = dynamic_cast<PerspectiveCamera*>(scene->m_mainCamera->getCamera())) {
                SceneView mainSceneView(*scene, *camera,
                    [](Entity* entity) {
                        return entity->getProperties() | EntityFlag_kVisible;
                    },
                    sceneRenderingOutput, 
                    { 0, 0, sceneRenderingOutput->width, sceneRenderingOutput->height }
                );
                // m_renderer->render(scene, mainSceneView, glm::uvec2(sceneRenderingOutput->width, sceneRenderingOutput->height));
            }

            // post scene rendering
            postSceneRenderingCallback(m_renderer.get(), sceneRenderingOutput);
        }
        // UI rendering
        m_renderer->renderUI();
        m_ctx->flip();
    }
}