#include "Modular.h"
#include "World.h"
#include "SceneCameraEntity.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"
#include "LightEntities.h"

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

        // add a camera
        Transform cameraTransform = { };
        cameraTransform.translation = glm::vec3(0.f, 4.f, 5.f); 
        SceneCameraEntity* ce = world->createEntity<SceneCameraEntity>("TestCamera_0", cameraTransform).get();
        auto cc = ce->getSceneCameraComponent();
        cc->setResolution(glm::uvec2(1920, 1080));
        cc->setRenderMode(SceneCamera::RenderMode::kSceneDirectLighting);
        auto cameraControllerComponent = std::make_shared<CameraControllerComponent>("CameraControllerComponent", cc);
        ce->addComponent(cameraControllerComponent);

        // add a directional light
        DirectionalLightEntity* dle = world->createEntity<DirectionalLightEntity>("SunLight", Transform()).get();
    }
}
