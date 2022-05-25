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
        if (!glfwInit())
        {
            cyanError("Error initializing glfw")
        }
        m_glfwWindow = glfwCreateWindow(m_windowDimension.x, m_windowDimension.y, "CyanEngine", nullptr, nullptr);
        glfwMakeContextCurrent(m_glfwWindow);

        // configure gl context
        if (glewInit())
        {
            cyanError("Error initializing glew")
        }
        // configure some gl global states
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(4.f);

        m_ctx = std::make_unique<GfxContext>(m_glfwWindow);
        m_sceneManager = std::make_unique<SceneManager>();
        m_textureManager = std::make_unique<TextureManager>();
        m_assetManager = std::make_unique<AssetManager>();
        m_shaderManager = std::make_unique<ShaderManager>();
        m_renderer = std::make_unique<Renderer>(m_ctx.get(), windowWidth, windowHeight);
        // m_lightMapManager = new LightMapManager;
        // m_pathTracer = new PathTracer;
    }

    void GraphicsSystem::initialize()
    {
        // initialize managers
        m_assetManager->initialize();
        m_shaderManager->initialize();
        m_renderer->initialize();

        // ui
        UI::initialize(m_glfwWindow);
    }

    void GraphicsSystem::finalize()
    {

    }

    void GraphicsSystem::update()
    {
        Scene* scene = m_scene.get();
        if (scene)
        {
            // update camera
            // todo: camera should be an entity and contains it's own simulation logic
            scene->camera.update();

            // update scene graph
            m_sceneManager->updateSceneGraph(scene);

            // (optional) update lighting
            // (optional) update material instance data
        }
    }
}