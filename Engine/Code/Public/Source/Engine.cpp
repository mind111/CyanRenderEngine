#include "Engine.h"

namespace Cyan
{
    Engine* Engine::get()
    {
        static std::unique_ptr<Engine> engine(new Engine());
        return engine.get();
    }

    Engine::Engine()
    {
    }

    void Engine::initialize()
    {
    }

    void Engine::deinitialize()
    {
    }

    void Engine::update()
    {
    }
}