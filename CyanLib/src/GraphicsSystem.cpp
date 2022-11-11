#include "GraphicsSystem.h"
#include "CyanUI.h"

namespace Cyan
{
    GraphicsSystem* Singleton<GraphicsSystem>::singleton = nullptr;

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

        // some important gl constants

        m_ctx = std::make_unique<GfxContext>(m_glfwWindow);
        m_sceneManager = std::make_unique<SceneManager>();
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
        UI::initialize(m_glfwWindow);
    }

    void GraphicsSystem::finalize()
    {

    }

    void GraphicsSystem::update(Scene* scene) {
        if (scene) {
            m_scene = scene;
        }
        m_scene->update();
    }

    void GraphicsSystem::render() {
        if (m_scene) {
            // clear default render target
            m_ctx->setRenderTarget(nullptr, { });
            m_ctx->clear();

            // todo: properly handle window resize here
            glm::uvec2 resolution = m_windowDimension;
            ITextureRenderable::Spec spec = { };
            spec.width = resolution.x;
            spec.height = resolution.y;
            spec.type = TEX_2D;
            spec.pixelFormat = PF_RGB16F;
            static Texture2DRenderable* frameOutput = new Texture2DRenderable("FrameOutput", spec);

            SceneView mainSceneView(*m_scene, m_scene->camera->getCamera(), EntityFlag_kVisible, frameOutput, { 0, 0, frameOutput->width, frameOutput->height });
            m_renderer->render(m_scene, mainSceneView);
            m_renderer->renderToScreen(mainSceneView.renderTexture);
            m_renderer->renderUI();

            m_ctx->flip();
        }
    }
}