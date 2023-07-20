#include "Modular.h"

int main()
{
    Cyan::Engine* engine = Cyan::Engine::create(std::move(std::make_unique<Cyan::ModularApp>(1920, 1080)));
    engine->initialize();
    engine->run();
    engine->deinitialize();
    return 0;
}
