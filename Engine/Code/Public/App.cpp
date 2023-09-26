#include "App.h"
#include "World.h"
#include "AssetManager.h"
#include "LightEntities.h"
#include "SkyboxEntity.h"
#include "SkyboxComponent.h"
#include "LightComponents.h"
#include "StaticMeshEntity.h"
#include "StaticMeshComponent.h"
#include "SceneCameraEntity.h"
#include "SceneCameraComponent.h"
#include "CameraControllerComponent.h"

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
        // add some default objects into the scene
        // add a camera
        Transform cameraTransform = { };
        cameraTransform.translation = glm::vec3(0.f, 2.f, 3.f); 
        SceneCameraEntity* ce = world->createEntity<SceneCameraEntity>("TestCamera_0", cameraTransform).get();
        auto cc = ce->getSceneCameraComponent();
        cc->setResolution(glm::uvec2(1920, 1080));
        cc->setRenderMode(SceneCamera::RenderMode::kResolvedSceneColor);
        auto cameraControllerComponent = std::make_shared<CameraControllerComponent>("CameraControllerComponent", cc);
        ce->addComponent(cameraControllerComponent);

        // add a directional light
        DirectionalLightEntity* dle = world->createEntity<DirectionalLightEntity>("SunLight", Transform()).get();

        // add a skybox
        SkyboxEntity* sbe = world->createEntity<SkyboxEntity>("Skybox", Transform()).get();
        SkyboxComponent* sbc = sbe->getSkyboxComponent();

        GHSampler2D HDRISampler;
        HDRISampler.setFilteringModeMag(SAMPLER2D_FM_BILINEAR);
        HDRISampler.setFilteringModeMin(SAMPLER2D_FM_BILINEAR);
        Texture2D* HDRI = AssetManager::createTexture2DFromImage(ENGINE_ASSET_PATH "HDRIs/neutral_sky.hdr", "SkyboxHDRI", HDRISampler);
        sbc->setHDRI(HDRI);

        // add a skylight
        SkyLightEntity* sle = world->createEntity<SkyLightEntity>("Skylight", Transform()).get();
        SkyLightComponent* slc = sle->getSkyLightComponent();
        slc->setHDRI(HDRI);

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
