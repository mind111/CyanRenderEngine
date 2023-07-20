#include "Modular.h"
#include "World.h"

namespace Cyan
{
    ModularApp::ModularApp(i32 windowWidth, i32 windowHeight)
        : App("Modular", windowWidth, windowHeight)
    {

    }

    ModularApp::~ModularApp()
    {

    }

    void ModularApp::customInitialize(World* world)
    {
        const char* sceneFilePath = ASSET_PATH "mesh/shader_balls.glb";
        world->import(sceneFilePath);
    }
}
