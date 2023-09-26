#include "DemoApp.h"
#include "World.h"
#include "SceneCameraEntity.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"
#include "LightEntities.h"

namespace Cyan
{
#define APP_ASSET_PATH "C:/dev/cyanRenderEngine/EngineApp/Apps/Demo/Resources/"

    DemoApp::DemoApp(i32 windowWidth, i32 windowHeight)
        : App("Demo", windowWidth, windowHeight)
    {

    }

    DemoApp::~DemoApp()
    {

    }

    void DemoApp::customInitialize(World* world)
    {
        const char* sceneFilePath = APP_ASSET_PATH "Scene.glb";
        // const char* sceneFilePath = APP_ASSET_PATH "Sponza.glb";
        // const char* sceneFilePath = APP_ASSET_PATH "SimplifiedSunTemple.glb";
        // const char* sceneFilePath = APP_ASSET_PATH "PicaPica.glb";
        // todo: find a simple archviz scene for demo
        world->import(sceneFilePath);
    }
}
