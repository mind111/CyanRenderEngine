#include <atomic>

#include "Engine.h"
#include "GfxModule.h"
#include "World.h"
#include "GfxInterface.h"

namespace Cyan
{
    Engine* Engine::s_instance = nullptr;

    Engine* Engine::create(std::unique_ptr<App> app)
    {
        static std::unique_ptr<Engine> engine(new Engine(std::move(app)));
        return engine.get();
    }

    Engine* Engine::get()
    {
        return s_instance;
    }

    Engine::Engine(std::unique_ptr<App> app)
    {
        m_app = std::move(app);
    }

    void Engine::initialize()
    {
        // initialize the gfx module
        m_gfx = GfxModule::create();
        m_gfx->initialize();
    }

    void Engine::deinitialize()
    {
    }

    void Engine::update()
    {
        m_app->update(m_world.get());
        m_world->update();
    }

    bool Engine::syncWithRendering()
    {
        i32 renderFrameNumber = m_gfx->m_renderFrameNumber.load();
        assert(renderFrameNumber <= m_gameFrameNumber);
        return renderFrameNumber >= (m_gameFrameNumber - 1);
    }

    void Engine::submitForRendering()
    {
        struct SceneView
        {
            IScene* scene;
            ISceneRenderer* renderer;
            ISceneRender* render;
            SceneViewState state;
        };
    }

    void Engine::run()
    {
        bRunning = true;
        while (bRunning)
        {
            // check if main thread is too far ahead of render thread
            if (syncWithRendering())
            {
                // do sim work on main thread
                update();
                // copy game state and submit work to render thread
                submitForRendering();

                m_gameFrameNumber++;
            }
        }
    }
}

#include "Modular.h"

int main()
{
    Cyan::Engine* engine = Cyan::Engine::create(std::move(std::make_unique<Cyan::ModularApp>("Modular", 1920, 1080)));
    engine->initialize();
    engine->run();
    engine->deinitialize();
    return 0;
}