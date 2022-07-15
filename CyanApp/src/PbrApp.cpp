#include <iostream>
#include <queue>
#include <functional>
#include <stdlib.h>
#include <thread>

#include "stb_image.h"
#include "ArHosekSkyModel.h"

#include "PbrApp.h"
#include "CyanUI.h"
#include "CyanAPI.h"
#include "Light.h"
#include "Shader.h"
#include "Mesh.h"

#define REBAKE_LIGHTMAP 0
#define MOUSE_PICKING   1

#if 0
namespace Demo
{
    void mouseScrollWheelCallback(double xOffset, double yOffset)
    {
        DemoApp* app = DemoApp::get();
        if (!app->mouseOverUI())
        {
            CameraCommand command = { };
            command.type = CameraCommand::Type::Zoom;
            command.mouseWheelChange = glm::dvec2(xOffset, yOffset);
            app->dispatchCameraCommand(command);
        }
    }

    void keyCallback(i32 key, i32 action)
    {

    }
}
#endif

#define SCENE_DEMO_00

DemoApp::DemoApp(u32 appWindowWidth, u32 appWindowHeight)
    : DefaultApp(appWindowWidth, appWindowHeight), bOrbit(false)
{
    activeDebugViewTexture = nullptr;
    m_currentDebugView = -1;
}

bool DemoApp::mouseOverUI()
{
    return (m_mouseCursorX < 400.f && m_mouseCursorX > 0.f && m_mouseCursorY < 720.f && m_mouseCursorY > 0.f);
}

void DemoApp::setupScene()
{
    using namespace Cyan;

    Cyan::Toolkit::GpuTimer timer("initDemoScene00()", true);

    auto sceneManager = Cyan::SceneManager::get();
    m_scene = sceneManager->importScene("demo_scene_00", "C:\\dev\\cyanRenderEngine\\scene\\demo_scene_00.json");

    auto graphicsSystem = gEngine->getGraphicsSystem();
    glm::uvec2 viewportSize = graphicsSystem->getAppWindowDimension();
    float aspectRatio = (float)viewportSize.x / viewportSize.y;
    Camera& camera = m_scene->camera;
    camera.aspectRatio = aspectRatio;
    camera.projection = glm::perspective(glm::radians(camera.fov), aspectRatio, camera.n, camera.f);

    Cyan::Scene* demoScene00 = m_scene.get();

    // lighting
    {
        // sun light
        /** note
            * 40 seems to be a good baseline for midday sun light
        */
        glm::vec4 sunColorAndIntensity(0.99, .98, 0.82, 25.0);
        demoScene00->createDirectionalLight("SunLight", /*direction=*/glm::vec3(1.0f, 0.9, 0.7f), /*colorAndIntensity=*/sunColorAndIntensity);

        // skybox
#if 0
        demoScene00->m_skybox = new Cyan::Skybox(nullptr, glm::uvec2(1024u), Cyan::SkyboxConfig::kUseProcedural);
        demoScene00->m_skybox->build();

        // light probes
        demoScene00->m_irradianceProbe = sceneManager->createIrradianceProbe(demoScene00, glm::vec3(0.f, 2.f, 0.f), glm::uvec2(512u, 512u), glm::uvec2(64u, 64u));
        demoScene00->m_reflectionProbe = sceneManager->createReflectionProbe(demoScene00, glm::vec3(0.f, 3.f, 0.f), glm::uvec2(2048u, 2048u));
#endif
    }

#if 1
    using PBR = Cyan::PBRMaterial;
    // helmet
    {
        auto helmetMatl = AssetManager::createMaterial<PBR>("helmet_matl");
        helmetMatl->parameter.albedo = Cyan::AssetManager::getAsset<Cyan::Texture2DRenderable>("helmet_diffuse");
        helmetMatl->parameter.normal = AssetManager::getAsset<Cyan::Texture2DRenderable>("helmet_nm");
        helmetMatl->parameter.occlusion = AssetManager::getAsset<Cyan::Texture2DRenderable>("helmet_ao");
        helmetMatl->parameter.metallicRoughness = AssetManager::getAsset<Cyan::Texture2DRenderable>("helmet_roughness");

        auto helmet = demoScene00->getEntity("DamagedHelmet");
        helmet->setMaterial("helmet_mesh", helmetMatl);
#if 0
        auto helmetMesh = helmet->getSceneComponent("helmet_mesh")->getAttachedMesh()->parent;
        helmetMesh->m_bvh = new Cyan::MeshBVH(helmetMesh);
        helmetMesh->m_bvh->build();
#endif
    }
    // bunnies
    {
        Entity* bunny0 = demoScene00->getEntity("Bunny0");
        Entity* bunny1 = demoScene00->getEntity("Bunny1");

        auto bunnyMatl = AssetManager::createMaterial<PBR>("BunnyMatl");
        bunnyMatl->parameter.kAlbedo = glm::vec3(0.855, 0.647, 0.125f);
        bunnyMatl->parameter.kRoughness = .1f;
        bunnyMatl->parameter.kMetallic = 1.0f;

        bunny0->setMaterial("bunny_mesh", bunnyMatl);
        bunny1->setMaterial("bunny_mesh", bunnyMatl);
    }
    // man
    {
        Cyan::Entity* man = demoScene00->getEntity("Man");
        auto manMatl = AssetManager::createMaterial<PBR>("ManMatl");
        manMatl->parameter.kAlbedo = glm::vec3(0.855, 0.855, 0.855);
        manMatl->parameter.kRoughness = .3f;
        manMatl->parameter.kMetallic = .3f;

        man->setMaterial("man_mesh", manMatl);
    }

    // grid of shader ball on the table
    {
        glm::vec3 gridLowerLeft(-2.1581, 1.1629, 2.5421);
        glm::vec3 defaultAlbedo(1.f);
        glm::vec3 silver(.753f, .753f, .753f);
        glm::vec3 gold(1.f, 0.843f, 0.f);
        glm::vec3 chrome(1.f);
        glm::vec3 plastic(0.061f, 0.757f, 0.800f);

        auto shaderBallMesh = AssetManager::getAsset<Mesh>("shaderball_mesh");
        PBR* shaderBallMatls[2][4] = { };
        shaderBallMatls[0][0] = AssetManager::createMaterial<PBR>("ShaderBallGold");
        shaderBallMatls[0][0]->parameter.albedo = AssetManager::getAsset<Texture2DRenderable>("default_checker_dark");
        shaderBallMatls[0][0]->parameter.kRoughness = 0.02f;
        shaderBallMatls[0][0]->parameter.kMetallic = 0.05f;
        for (u32 j = 0; j < 2; ++j)
        {
            for (u32 i = 0; i < 4; ++i)
            {
                char entityName[32];
                sprintf_s(entityName, "ShaderBall%u", j * 4 + i);
                glm::vec3 posOffset = glm::vec3(1.3f * (f32)i, 0.f, -1.5f * (f32)j);
                Transform transform = { };
                transform.m_translate = gridLowerLeft + posOffset;
                transform.m_scale = glm::vec3(.003f);
                auto shaderBall = demoScene00->createStaticMeshEntity(entityName, transform, shaderBallMesh);
                shaderBall->setMaterial(shaderBallMatls[0][0]);
            }
        }
    }
#endif
    // room
    {
#if 0
        Cyan::Entity* room = demoScene00->getEntity("Room");
        room->setMaterial("room_mesh", defaultMatl);
        room->setMaterial("plane_mesh", defaultMatl);
        Cyan::Mesh* roomMesh = roomNode->m_meshInstance->m_mesh;
        Cyan::Mesh* planeMesh = planeNode->m_meshInstance->m_mesh;
        roomMesh->m_bvh = new Cyan::MeshBVH(roomMesh);
        roomMesh->m_bvh->build();
#endif

#if REBAKE_LIGHTMAP
        // default matl param
        Cyan::PbrMaterialParam params = { };
        params.flatBaseColor = glm::vec4(1.f);
        params.kRoughness = 0.8f;
        params.kMetallic = 0.02f;
        params.hasBakedLighting = 1.f;
        for (u32 sm = 0; sm < roomMesh->numSubMeshes(); ++sm)
        {

            i32 matlIdx = roomMesh->m_subMeshes[sm]->m_materialIdx;
            if (matlIdx >= 0)
            {
                auto& objMatl = roomMesh->m_objMaterials[matlIdx];
                params.flatBaseColor = glm::vec4(objMatl.diffuse, 1.f);
            }
            auto matl = createStandardPbrMatlInstance(demoScene00, params, room->m_static);
            room->setMaterial("RoomMesh", sm, matl);
        }
        params.usePrototypeTexture = 1.f;
        auto planeMatl = createStandardPbrMatlInstance(demoScene00, params, room->m_static);
        room->setMaterial("PlaneMesh", -1, planeMatl);

        // bake lightmap
        auto lightMapManager = Cyan::LightMapManager::get();
        lightMapManager->bakeLightMap(m_scenes[Scenes::Demo_Scene_00], roomNode, true);
        lightMapManager->bakeLightMap(m_scenes[Scenes::Demo_Scene_00], planeNode, true);

        for (u32 sm = 0; sm < roomMesh->numSubMeshes(); ++sm)
        {
            auto matl = roomNode->m_meshInstance->m_matls[sm];
            matl->bindTexture("lightMap", roomNode->m_meshInstance->m_lightMap->m_texAltas);
        }
        for (u32 sm = 0; sm < planeMesh->numSubMeshes(); ++sm)
        {
            auto matl = planeNode->m_meshInstance->m_matls[sm];
            matl->bindTexture("lightMap", planeNode->m_meshInstance->m_lightMap->m_texAltas);
        }
#else
        // directly use baked lightmap
#if 0
        auto lightMapManager = Cyan::LightMapManager::get();
        lightMapManager->createLightMapFromTexture(roomNode, textureManager->getTexture("RoomMesh_lightmap"));
        lightMapManager->createLightMapFromTexture(planeNode, textureManager->getTexture("PlaneMesh_lightmap"));
        for (u32 sm = 0; sm < roomMesh->numSubMeshes(); ++sm)
        {
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.uniformRoughness", .8f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.uniformMetallic", .02f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMaterialProps.hasBakedLighting", 1.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.indirectDiffuseScale", 0.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.indirectSpecularScale", 0.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMaterialProps.hasBakedLighting", 1.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.flags", Cyan::StandardPbrMaterial::Flags::kUseLightMap);
            roomNode->m_meshInstance->m_matls[sm]->bindTexture("lightMap", roomNode->m_meshInstance->m_lightMap->m_texAltas);
        }
        Cyan::PbrMaterialParam params = { };
        params.flatBaseColor = glm::vec4(1.f);
        params.kRoughness = 0.8f;
        params.kMetallic = 0.0f;
        params.hasBakedLighting = 1.f;
        params.usePrototypeTexture = 1.f;
        params.lightMap = planeNode->m_meshInstance->m_lightMap->m_texAltas;

        auto planeMatl = assetManager->createMaterial<Cyan::LightMappedMaterial<PBR>>("RoomMaterial");

        // auto planeMatl = createStandardPbrMatlInstance(demoScene00, params, room->m_static);
        room->setMaterial("PlaneMesh", -1, planeMatl);
        sceneManager->updateSceneGraph(demoScene00);
#endif
#endif
    }
    timer.end();
}

void DemoApp::customInitialize()
{
    Cyan::Toolkit::GpuTimer timer("DemoApp::customInitialize()", true);
    {
        // setup scene
        setupScene();
    }
    timer.end();
}

void DemoApp::precompute()
{
    // build lighting
#ifdef SCENE_DEMO_00 
    m_scene->irradianceProbe->build();
    m_scene->reflectionProbe->build();
#endif
#ifdef SCENE_SPONZA
    auto pathTracer = Cyan::PathTracer::get();
    pathTracer->setScene(m_scenes[Sponza_Scene]);
    pathTracer->run(m_scenes[Sponza_Scene]->getActiveCamera());
#endif
#if 0
#ifdef SCENE_DEMO_00
    auto pathTracer = Cyan::PathTracer::get();
    pathTracer->setScene(m_scenes[Demo_Scene_00]);
    pathTracer->run(m_scenes[Demo_Scene_00]->getActiveCamera());
    pathTracer->fillIrradianceCacheDebugData(m_scenes[Demo_Scene_00]->getActiveCamera());
    m_debugLines.resize(pathTracer->m_irradianceCache->m_numRecords);
    glm::vec4 color(0.f, 0.f, 1.f, 1.f);
    for (u32 i = 0; i < m_debugLines.size(); ++i)
    {
        auto& record = pathTracer->m_irradianceCache->m_records[i];
        auto& translationalGradient = pathTracer->m_debugData.translationalGradients[i];
        m_debugLines[i].init();
        m_debugLines[i].setVertices(record.position, record.position + translationalGradient);
        m_debugLines[i].setColor(color);
    }
#endif
#endif
}

void DemoApp::customUpdate()
{

}

void DemoApp::customRender()
{

}

void DemoApp::customFinalize()
{

}

// entry point
int main()
{
    DemoApp* app = new DemoApp(1280, 720);
    app->initialize();
    app->run();
    app->finalize();
    delete app;
    return 0;
}
