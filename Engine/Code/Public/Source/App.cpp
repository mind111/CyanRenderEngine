#include "App.h"
#include "World.h"

namespace Cyan
{
    App::App(const char* name, int windowWidth, int windowHeight)
        : m_name(name), m_windowWidth(windowWidth), m_windowHeight(windowHeight), bRunning(false)
    {
        m_engine = Engine::get();
        m_world = std::make_unique<World>("Default");
    }

    void App::initalize()
    {
        m_engine->initialize();

        customInitialize();
    }

    void App::deinitialize()
    {
        m_engine->deinitialize();

        customDeinitialize();
    }

    void App::run()
    {
        bRunning = true;
        while (bRunning)
        {
            update();
            render();
        }
    }

    void App::update()
    {
        m_engine->update();
        m_world->update();
        customUpdate();
    }

    void App::render()
    {
        customRender();
    }
}
