#include "App.h"
#include "World.h"

namespace Cyan
{
    App::App(const char* name, int windowWidth, int windowHeight)
        : m_name(name), m_windowWidth(windowWidth), m_windowHeight(windowHeight)
    {
    }

    App::~App()
    {

    }

    void App::initialize(World* world)
    {
        customInitialize(world);
    }

    void App::deinitialize()
    {
        customDeinitialize();
    }

    void App::update(World* world)
    {
    }

    void App::render()
    {
    }

    void App::customInitialize(World* world)
    {

    }

    void App::customDeinitialize()
    {

    }
}
