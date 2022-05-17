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

// In radians per pixel 
static float kCameraOrbitSpeed = 0.005f;
static float kCameraRotateSpeed = 0.005f;

struct CameraControlInputs
{
    double mouseCursorDx;
    double mouseCursorDy;
    double mouseWheelDx;
    double mouseWheelDy;
};

struct CameraCommand
{
    enum Type
    {
        Orbit = 0,
        Zoom,
        Undefined
    };

    Type type;
    CameraControlInputs params;
};

CameraCommand buildCameraCommand(CameraCommand::Type type, double mouseCursorDx, double mouseCursorDy, double mouseWheelDx, double mouseWheelDy)
{
    return CameraCommand{ type, { mouseCursorDx, mouseCursorDy, mouseWheelDx, mouseWheelDy }};
}

void DemoApp::dispatchCameraCommand(CameraCommand& command)
{
    DemoApp* app = DemoApp::get();
    switch(command.type)
    {
        case CameraCommand::Type::Orbit:
        {
            Camera& camera = m_scenes[m_currentScene]->cameras[0];
            app->orbitCamera(camera, command.params.mouseCursorDx, command.params.mouseCursorDy);
        } break;
        case CameraCommand::Type::Zoom:
        {
            Camera& camera = m_scenes[m_currentScene]->cameras[0];
            app->zoomCamera(camera, command.params.mouseWheelDx, command.params.mouseWheelDy);
        } break;
        default:
            break;
    }
}

namespace Demo
{
    void mouseCursorCallback(double cursorX, double cursorY, double deltaX, double deltaY)
    {
        DemoApp* app = DemoApp::get();
        CameraCommand command = { };
        command.type = CameraCommand::Type::Orbit;
        command.params.mouseCursorDx = deltaX;
        command.params.mouseCursorDy = deltaY;

        app->bOrbit ? app->dispatchCameraCommand(command) : app->rotateCamera(deltaX, deltaY);

        app->m_mouseCursorX = cursorX;
        app->m_mouseCursorY = cursorY;
    }

    void mouseButtonCallback(int button, int action)
    {
        DemoApp* app = DemoApp::get();
        switch(button)
        {
            case CYAN_MOUSE_BUTTON_LEFT:
            {
                if (action == CYAN_PRESS)
                {
#if MOUSE_PICKING
                    if (!app->mouseOverUI())
                    {
                        app->bRayCast = true;
                    }
#endif
                }
                else if (action == CYAN_RELEASE)
                {
                    if (!app->mouseOverUI())
                    {
                        app->bRayCast = false;
                    }
                }
                break;
            }
            case CYAN_MOUSE_BUTTON_RIGHT:
            {
                if (action == CYAN_PRESS)
                {
                    app->bOrbit = true;
                }
                else if (action == CYAN_RELEASE)
                {
                    app->bOrbit = false;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }

    void mouseScrollWheelCallback(double xOffset, double yOffset)
    {
        DemoApp* app = DemoApp::get();
        if (!app->mouseOverUI())
        {
            CameraCommand command = { };
            command.type = CameraCommand::Type::Zoom;
            command.params.mouseWheelDx = xOffset;
            command.params.mouseWheelDy = yOffset;
            app->dispatchCameraCommand(command);
        }
    }

    void keyCallback(i32 key, i32 action)
    {
        DemoApp* app = DemoApp::get();
        switch (key)
        {
            // switch camera view
            case GLFW_KEY_P: 
            {
                if (action == GLFW_PRESS)
                {
                    app->switchCamera();
                }
            } break; 
            default :
            {

            }
        }
    }
}

#define SCENE_DEMO_00
//#define SCENE_SPONZA

enum Scenes
{
    Demo_Scene_00 = 0,
    Sponza_Scene,
    kCount
};

static DemoApp* gApp = nullptr;

DemoApp* DemoApp::get()
{
    if (gApp)
        return gApp;
    return nullptr;
}

DemoApp::DemoApp()
: bOrbit(false)
{
    bOrbit = false;
    gApp = this;
    m_scenes.resize(Scenes::kCount, nullptr);
    activeDebugViewTexture = nullptr;
    m_currentDebugView = -1;
}

bool DemoApp::mouseOverUI()
{
    return (m_mouseCursorX < 400.f && m_mouseCursorX > 0.f && m_mouseCursorY < 720.f && m_mouseCursorY > 0.f);
}

#if 0
Cyan::StandardPbrMaterial* DemoApp::createStandardPbrMatlInstance(Scene* scene, Cyan::PbrMaterialParam params, bool isStatic)
{
    if (isStatic)
    {
        params.indirectDiffuseScale = 0.f;
        params.indirectSpecularScale = 0.f;
    }
    else
    {
        params.indirectDiffuseScale = 1.f;
        params.indirectSpecularScale = 1.f;
    }
    Cyan::StandardPbrMaterial* matl = new Cyan::StandardPbrMaterial(params);
    scene->addStandardPbrMaterial(matl);
    return matl;
}
#endif

void DemoApp::createHelmetInstance(Scene* scene)
{
    auto textureManager = m_graphicsSystem->getTextureManager();
    auto sceneManager = SceneManager::get();
#if 0
    // helmet
    {
        Cyan::PbrMaterialParam params = { };
        params.baseColor = textureManager->getTexture("helmet_diffuse");
        params.normal = textureManager->getTexture("helmet_nm");
        params.metallicRoughness = textureManager->getTexture("helmet_roughness");
        params.occlusion = textureManager->getTexture("helmet_ao");
        auto helmetMatl = createStandardPbrMatlInstance(scene, params, false);
        Entity* helmet = sceneManager->getEntity(scene, "DamagedHelmet");
        helmet->setMaterial("HelmetMesh", 0, helmetMatl);
    }
#endif
}

void DemoApp::initDemoScene00()
{
    Cyan::Toolkit::GpuTimer timer("initDemoScene00()", true);
    auto textureManager = m_graphicsSystem->getTextureManager();
    auto sceneManager = SceneManager::get();
    m_scenes[Scenes::Demo_Scene_00] = sceneManager->createScene("demo_scene_00", "C:\\dev\\cyanRenderEngine\\scene\\demo_scene_00.json");
    Scene* demoScene00 = m_scenes[Scenes::Demo_Scene_00];

    using PBR = Cyan::PBRMatl;
    auto assetManager = Cyan::AssetManager::get();
    PBR* defaultMatl = assetManager->createMaterial<PBR>("DefaultMatl");
    defaultMatl->parameter.kRoughness = .8f;
    defaultMatl->parameter.kMetallic = .02f;
    // helmet
    {
        createHelmetInstance(demoScene00);
        auto helmet = sceneManager->getEntity(demoScene00, "DamagedHelmet");
        auto helmetMesh = helmet->getSceneNode("helmet_mesh")->getAttachedMesh()->parent;
#if 0
        helmetMesh->m_bvh = new Cyan::MeshBVH(helmetMesh);
        helmetMesh->m_bvh->build();
#endif
    }
    // bunnies
    {
        Entity* bunny0 = sceneManager->getEntity(demoScene00, "Bunny0");
        Entity* bunny1 = sceneManager->getEntity(demoScene00, "Bunny1");

        auto bunnyMatl = assetManager->createMaterial<PBR>("BunnyMatl");
        bunnyMatl->parameter.kAlbedo = glm::vec3(0.855, 0.647, 0.125f);
        bunnyMatl->parameter.kRoughness = .1f;
        bunnyMatl->parameter.kMetallic = 1.0f;

        bunny0->setMaterial("bunny_mesh", -1, bunnyMatl);
        bunny1->setMaterial("bunny_mesh", -1, bunnyMatl);
    }
    // man
    {
        Entity* man = sceneManager->getEntity(demoScene00, "Man");
        auto manMatl = assetManager->createMaterial<PBR>("ManMatl");
        manMatl->parameter.kAlbedo = glm::vec3(0.855, 0.855, 0.855);
        manMatl->parameter.kRoughness = .3f;
        manMatl->parameter.kMetallic = .3f;

        man->setMaterial("man_mesh", -1, manMatl);
    }
    // lighting
    // todo: refactor lights, decouple them from Entity
    {
        // sun light
        glm::vec3 sunDir = glm::normalize(glm::vec3(1.0f, 0.5f, 1.8f));
        sceneManager->createDirectionalLight(demoScene00, glm::vec3(1.0f, 0.9, 0.7f), sunDir, 3.6f);

        // skybox
        demoScene00->m_skybox = new Cyan::Skybox(nullptr, glm::uvec2(1024u), Cyan::SkyboxConfig::kUseProcedural);
        demoScene00->m_skybox->build();

        // light probes
        demoScene00->m_irradianceProbe = sceneManager->createIrradianceProbe(demoScene00, glm::vec3(0.f, 2.f, 0.f), glm::uvec2(512u, 512u), glm::uvec2(64u, 64u));
        demoScene00->m_reflectionProbe = sceneManager->createReflectionProbe(demoScene00, glm::vec3(0.f, 3.f, 0.f), glm::uvec2(2048u, 2048u));
    }

    // grid of shader ball on the table
    {
        glm::vec3 gridLowerLeft(-2.1581, 1.1629, 2.5421);
        glm::vec3 defaultAlbedo(1.f);
        glm::vec3 silver(.753f, .753f, .753f);
        glm::vec3 gold(1.f, 0.843f, 0.f);
        glm::vec3 chrome(1.f);
        glm::vec3 plastic(0.061f, 0.757f, 0.800f);

        PBR* shaderBallMatls[2][4] = { };
        shaderBallMatls[0][0] = assetManager->createMaterial<PBR>("ShaderBallGold");
        shaderBallMatls[0][0]->parameter.kRoughness = 0.02f;
        shaderBallMatls[0][0]->parameter.kMetallic = 0.05f;
/*
        matlParams[0][3].kRoughness = .02f;
        matlParams[0][3].kMetallic = 0.05f;
        matlParams[0][3].kSpecular = 1.f;
        matlParams[1][0].flatBaseColor = glm::vec4(gold, 1.f);
        matlParams[1][0].roughness = textureManager->getTexture("imperfection_grunge");
        matlParams[1][0].kMetallic = 0.95f;
        matlParams[1][1].baseColor = textureManager->getTexture("green_marble_albedo");
        matlParams[1][1].roughness = textureManager->getTexture("green_marble_roughness");
        matlParams[1][1].kMetallic = 0.01f;
        matlParams[1][2].baseColor = textureManager->getTexture("white_marble_albedo");
        matlParams[1][2].roughness = textureManager->getTexture("white_marble_roughness");
        matlParams[1][2].kMetallic = 0.01f;
        matlParams[1][3].baseColor = textureManager->getTexture("brick_albedo");
        matlParams[1][3].roughness = textureManager->getTexture("brick_roughness");
        matlParams[1][3].normal = textureManager->getTexture("brick_nm");
        matlParams[1][3].kMetallic = 0.01f;
        matlParams[1][3].kSpecular = 0.01f;

        Cyan::PbrMaterialParam matlParams[2][4] = { };
        matlParams[0][0].flatBaseColor = glm::vec4(gold, 1.f);
        matlParams[0][0].kRoughness = .1f;
        matlParams[0][0].kMetallic = 0.999f;
        matlParams[0][1].flatBaseColor = glm::vec4(silver, 1.f);
        matlParams[0][1].kRoughness = .02f;
        matlParams[0][1].kMetallic = 1.0;
        matlParams[0][2].flatBaseColor = glm::vec4(chrome, 1.f);
        matlParams[0][2].kRoughness = .3f;
        matlParams[0][2].kMetallic = 0.999f;
        matlParams[0][3].flatBaseColor = glm::vec4(plastic, 1.f);
        matlParams[0][3].kRoughness = .02f;
        matlParams[0][3].kMetallic = 0.05f;
        matlParams[0][3].kSpecular = 1.f;
        matlParams[1][0].flatBaseColor = glm::vec4(gold, 1.f);
        matlParams[1][0].roughness = textureManager->getTexture("imperfection_grunge");
        matlParams[1][0].kMetallic = 0.95f;
        matlParams[1][1].baseColor = textureManager->getTexture("green_marble_albedo");
        matlParams[1][1].roughness = textureManager->getTexture("green_marble_roughness");
        matlParams[1][1].kMetallic = 0.01f;
        matlParams[1][2].baseColor = textureManager->getTexture("white_marble_albedo");
        matlParams[1][2].roughness = textureManager->getTexture("white_marble_roughness");
        matlParams[1][2].kMetallic = 0.01f;
        matlParams[1][3].baseColor = textureManager->getTexture("brick_albedo");
        matlParams[1][3].roughness = textureManager->getTexture("brick_roughness");
        matlParams[1][3].normal = textureManager->getTexture("brick_nm");
        matlParams[1][3].kMetallic = 0.01f;
        matlParams[1][3].kSpecular = 0.01f;
*/
        using namespace Cyan;
        for (u32 j = 0; j < 2; ++j)
        {
            for (u32 i = 0; i < 4; ++i)
            {
                char entityName[32];
                char meshNodeName[32];
                sprintf_s(entityName, "ShaderBall%u", j * 4 + i);
                sprintf_s(meshNodeName, "ShaderBall%u", j * 4 + i);
                auto shaderBall = sceneManager->createEntity(demoScene00, entityName, Transform{}, false);

                glm::vec3 posOffset = glm::vec3(1.3f * (f32)i, 0.f, -1.5f * (f32)j);
                Transform transform = { };
                transform.m_translate = gridLowerLeft + posOffset;
                transform.m_scale = glm::vec3(.003f);
                auto meshNode = sceneManager->createMeshNode(demoScene00, transform, assetManager->getAsset<Mesh>("shaderball_mesh"));
                shaderBall->attachSceneNode(meshNode);
                shaderBall->setMaterial(meshNodeName, -1, defaultMatl);
                shaderBall->setMaterial(meshNodeName, 0, defaultMatl);
                shaderBall->setMaterial(meshNodeName, 1, defaultMatl);
                shaderBall->setMaterial(meshNodeName, 5, defaultMatl);
            }
        }
    }
    // room
    {
        Entity* room = sceneManager->getEntity(demoScene00, "Room");
        room->setMaterial("room_mesh", -1, defaultMatl);
        room->setMaterial("plane_mesh", -1, defaultMatl);
#if 0
        Entity* room = sceneManager->getEntity(demoScene00, "Room");
        auto roomNode = room->getSceneNode("RoomMesh");
        auto planeNode = room->getSceneNode("PlaneMesh");
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

void DemoApp::initSponzaScene()
{
    Cyan::Toolkit::GpuTimer timer("initSponzaScene()", true);
    auto sceneManager = SceneManager::get();
    m_scenes[Scenes::Sponza_Scene] = sceneManager->createScene("sponza_scene", "C:\\dev\\cyanRenderEngine\\scene\\sponza_scene.json");
    Scene* sponzaScene = m_scenes[Scenes::Sponza_Scene];
    m_currentScene = static_cast<u32>(Scenes::Sponza_Scene);

    Entity* sponza = sceneManager->getEntity(sponzaScene, "Sponza");
    auto sponzaNode = sponza->getSceneNode("SponzaMesh");
    Cyan::Mesh* sponzaMesh = sponzaNode->getAttachedMesh()->parent;
#if 0
    sponzaMesh->m_bvh = new Cyan::MeshBVH(sponzaMesh);
    sponzaMesh->m_bvh->build();
#endif
    // lighting
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
    sceneManager->createDirectionalLight(sponzaScene, glm::vec3(1.0f, 0.9, 0.7f), sunDir, 3.6f);

    // light probes
    sponzaScene->m_irradianceProbe = sceneManager->createIrradianceProbe(sponzaScene, glm::vec3(0.f, 2.f, 0.f), glm::uvec2(512u, 512u), glm::uvec2(64u, 64u));
    sponzaScene->m_reflectionProbe = sceneManager->createReflectionProbe(sponzaScene, glm::vec3(0.f, 3.f, 0.f), glm::uvec2(2048u, 2048u));
}

void DemoApp::initialize(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize)
{
    Cyan::Toolkit::GpuTimer timer("DemoApp::initialize()", true);

    bRunning = true;
    // init engine
    gEngine->initialize({ appWindowWidth, appWindowHeight }, sceneViewportPos, renderSize);
    // setup input control
    gEngine->registerMouseCursorCallback(&Demo::mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&Demo::mouseButtonCallback);
    gEngine->registerMouseScrollWheelCallback(&Demo::mouseScrollWheelCallback);
    gEngine->registerKeyCallback(&Demo::keyCallback);

    m_graphicsSystem = Cyan::GraphicsSystem::get();

    // physically-based shading shader
    Cyan::createShader("PBSShader", SHADER_SOURCE_PATH "pbs_v.glsl", SHADER_SOURCE_PATH "pbs_p.glsl");

    auto renderer = m_graphicsSystem->getRenderer();
    glm::vec2 viewportSize(renderer->m_windowWidth, renderer->m_windowHeight);
    float aspectRatio = viewportSize.x / viewportSize.y;
    auto initializeCamera = [&](Scene* scene)
    {
        for (auto& camera : scene->cameras)
        {
            camera.aspectRatio = aspectRatio;
            camera.projection = glm::perspective(glm::radians(camera.fov), aspectRatio, camera.n, camera.f);
        }
    };
#ifdef SCENE_DEMO_00
    {
        initDemoScene00();
        initializeCamera(m_scenes[Scenes::Demo_Scene_00]);
        m_currentScene = Scenes::Demo_Scene_00;
    }
#endif

    // misc
    bRayCast = false;
    bPicking = false;
    m_selectedEntity = nullptr;
    m_debugViewIndex = 0u;
    bDisneyReparam = true;
    m_directDiffuseSlider = 1.f;
    m_directSpecularSlider = 1.f;
    m_indirectDiffuseSlider = 1.f;
    m_indirectSpecularSlider = 4.0f;
    m_directLightingSlider = 1.f;
    m_indirectLightingSlider = 0.f;

    // clear color
    Cyan::getCurrentGfxCtx()->setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.f));

    timer.end();
}

void DemoApp::precompute()
{
    // build lighting
#ifdef SCENE_DEMO_00 
    m_scenes[Scenes::Demo_Scene_00]->m_irradianceProbe->build();
    m_scenes[Scenes::Demo_Scene_00]->m_reflectionProbe->build();
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
    auto shader = Cyan::createShader("DebugShadingShader", SHADER_SOURCE_PATH "debug_color_vs.glsl", SHADER_SOURCE_PATH "debug_color_fs.glsl");
}

void DemoApp::run()
{
    precompute();
    while (bRunning)
    {
        update();
        render();
    }
}

// rotate p around c using quaternion
glm::mat4 rotateAroundPoint(glm::vec3 c, glm::vec3 axis, float degree)
{
    glm::mat4 result(1.f);
    result = glm::translate(result, c * -1.f);
    float theta = glm::radians(degree);
    glm::quat qRotation(cosf(theta * .5f), glm::normalize(axis) * sinf(theta * .5f));
    glm::mat4 rotation = glm::toMat4(qRotation);
    result = rotation * result;
    result = glm::translate(result, c);
    return result;
}

glm::vec3 computeMouseHitWorldSpacePos(Camera& camera, glm::vec3 rd, RayCastInfo hitInfo)
{
    glm::vec3 viewSpaceHit = hitInfo.t * rd;
    glm::mat4 invViewMat = glm::inverse(camera.view);
    glm::vec4 worldSpacePosV4 = invViewMat * glm::vec4(viewSpaceHit, 1.f);
    return glm::vec3(worldSpacePosV4.x, worldSpacePosV4.y, worldSpacePosV4.z);
};

RayCastInfo DemoApp::castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize)
{
    // convert mouse cursor pos to view space 
    glm::vec2 viewportPos = gEngine->getSceneViewportPos();
    double mouseCursorX = m_mouseCursorX - currentViewportPos.x;
    double mouseCursorY = m_mouseCursorY - currentViewportPos.y;
    // NDC space
    glm::vec2 uv(2.0 * mouseCursorX / currentViewportSize.x - 1.0f, 2.0 * (currentViewportSize.y - mouseCursorY) / currentViewportSize.y - 1.0f);
    // homogeneous clip space
    glm::vec4 q = glm::vec4(uv, -0.8, 1.0);
    Camera& camera = m_scenes[m_currentScene]->getActiveCamera();
    glm::mat4 projInverse = glm::inverse(camera.projection);
    // q.z must be less than zero after this evaluation
    q = projInverse * q;
    // view space
    q /= q.w;
    glm::vec3 rd = glm::normalize(glm::vec3(q.x, q.y, q.z));
    // in view space
    glm::mat4 viewInverse = glm::inverse(camera.view);

    glm::vec3 ro = glm::vec3(0.f);

    RayCastInfo closestHit;

    // ray intersection test against all the entities in the scene to find the closest intersection
    for (auto entity : m_scenes[m_currentScene]->entities)
    {
        if (entity->m_visible && entity->m_selectable)
        {
#if 0
            RayCastInfo traceInfo = entity->intersectRay(ro, rd, camera.view); 
            if (traceInfo.t > 0.f && traceInfo < closestHit)
            {
                closestHit = traceInfo;
                printf("Cast a ray from mouse that hits %s \n", traceInfo.m_node->m_name);
            }
#endif
        }
    }
    glm::vec3 worldHit = computeMouseHitWorldSpacePos(camera, rd, closestHit);
    printf("Mouse hit world at (%.2f, %.2f, %.2f) \n", worldHit.x, worldHit.y, worldHit.z);
    return closestHit;
}

void DemoApp::updateScene(Scene* scene)
{
    // update camera
    u32 camIdx = 0u;
    Camera& camera = m_scenes[m_currentScene]->cameras[camIdx];
    camera.update();
    SceneManager::get()->updateSceneGraph(m_scenes[m_currentScene]);
}

void DemoApp::update()
{
    gEngine->processInput();
    updateScene(m_scenes[m_currentScene]);
}

void uiDrawSceneNode(SceneNode* node)
{
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    if (node)
    {
        if (node->m_child.size() > 0)
        {
            if (ImGui::TreeNodeEx(node->m_name, baseFlags))
            {
                for (auto child : node->m_child)
                {
                    uiDrawSceneNode(child);
                }
                ImGui::TreePop();
            }
        }
        else
        {
            ImGuiTreeNodeFlags nodeFlags = baseFlags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::TreeNodeEx(node->m_name, nodeFlags);
        }
    }
};

void DemoApp::drawEntityPanel()
{
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    // transform panel
    if (ImGui::TreeNodeEx("Transform", baseFlags))
    {
        Transform transform = m_selectedEntity->getWorldTransform();
        ImGui::Text("Translation");
        ImGui::SameLine();
        ImGui::InputFloat3("##Translation", &transform.m_translate.x);
        ImGui::Text("Rotation");
        ImGui::SameLine();
        ImGui::InputFloat4("##Rotation",  &transform.m_qRot.x);
        ImGui::Text("Scale");
        ImGui::SameLine();
        ImGui::InputFloat3("##Scale", &transform.m_scale.x);
        ImGui::TreePop();
    }
    ImGui::Separator();

    // scene component panel
    if (ImGui::TreeNodeEx("SceneComponent", baseFlags))
    {
        SceneNode* root = m_selectedEntity->m_sceneRoot;
        uiDrawSceneNode(root);
        ImGui::TreePop();
    }
}

void DemoApp::drawDebugWindows()
{
#if 0
    auto renderer = m_graphicsSystem->getRenderer();
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setRenderTarget(nullptr, {});

    // configure window pos and size
    ImVec2 debugWindowSize(gEngine->getWindow().width - renderer->m_windowWidth, 
                           gEngine->getWindow().height);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(debugWindowSize);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    Cyan::UI::beginWindow("Editor", windowFlags);
    ImGui::PushFont(Cyan::UI::gFont);
    {
        // multiple tabs
        ImGuiTabBarFlags tabFlags = ImGuiTabBarFlags_Reorderable;
        if (ImGui::BeginTabBar("MyTabBar", tabFlags))
        {
            if (ImGui::BeginTabItem("Scene"))
            {
                {
                    drawStats();
                    ImGui::Separator();
                }
                {
                    Camera& camera = m_scenes[m_currentScene]->getActiveCamera();
                    ImGui::Text("Camera");
                    ImGui::Indent();
                    ImGui::Text("Position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);
                    ImGui::Text("Look at:  %.2f, %.2f, %.2f", camera.lookAt.x, camera.lookAt.y, camera.lookAt.z);
                    ImGui::Unindent();
                    ImGui::Separator();
                }
                {
                    uiDrawEntityGraph(m_scenes[m_currentScene]->m_rootEntity);
                    ImGui::Separator();
                }
                if (m_selectedEntity)
                {
                    drawEntityPanel();
                    ImGui::Separator();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Lighting"))
            {
                drawLightingWidgets();
                ImGui::Separator();
                drawRenderSettings();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Assets"))
            {
                // textures
                {
                    auto textureManager = Cyan::TextureManager::get();
                    i32 currentItem = 0;
                    u32 numTextures = textureManager->getNumTextures();
                    std::vector<const char*> textureNames(numTextures);
                    std::vector<Cyan::Texture*>& textures = textureManager->s_textures;
                    for (u32 i = 0; i < numTextures; ++i)
                        textureNames[i] = textures[i]->name.c_str();
                    ImGui::Text("Textures");
                    ImGui::Text("Number of textures: %u", numTextures);
                    ImGui::ListBox("##Textures", &currentItem, textureNames.data(), textureNames.size());
                    ImGui::Separator();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Debug"))
            {
                auto renderer = Cyan::Renderer::get();
                u32 numDebugViews = 0;
                auto buildDebugView = [&](const char* viewName, Cyan::Texture* texture, u32 width=320, u32 height=180, bool flip = true)
                {
                    ImGui::Text(viewName);
                    if (flip)
                        ImGui::Image((ImTextureID)texture->handle, ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));
                    else
                        ImGui::Image((ImTextureID)texture->handle, ImVec2(width, height));
                    char buttonName[64];
                    sprintf_s(buttonName, "Focus on view##%s", viewName);
                    if (ImGui::Button(buttonName))
                    {
                        if (m_currentDebugView == numDebugViews)
                        {
                            activeDebugViewTexture = nullptr;
                            m_currentDebugView = -1;
                        }
                        else
                        {
                            m_currentDebugView = numDebugViews;
                            activeDebugViewTexture = texture;
                        }
                    }
                    ImGui::Separator();
                    numDebugViews++;
                };
                // buildDebugView("PathTracing", Cyan::PathTracer::get()->getRenderOutput());
#ifdef SCENE_DEMO_00
                Entity* room = SceneManager::get()->get()->getEntity(m_scenes[Demo_Scene_00], "Room");
                SceneNode* sceneNode = room->getSceneNode("RoomMesh");
                // buildDebugView("LightMap", sceneNode->m_meshInstance->m_lightMap->m_texAltas, 320u, 320u);
#endif
                buildDebugView("SceneDepth",  renderer->m_sceneDepthTextureSSAA);
                buildDebugView("SceneNormal", renderer->m_sceneNormalTextureSSAA);
                buildDebugView("SceneGTAO",   renderer->m_ssaoTexture);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::PopFont();
    Cyan::UI::endWindow();
#endif
}

void DemoApp::drawStats()
{
    {
        ImGui::Text("Frame time:                   %.2f ms", m_lastFrameDurationInMs);
        ImGui::Text("Number of entities:           %d", m_scenes[m_currentScene]->entities.size());
        ImGui::Checkbox("4x Super Sampling", &Cyan::Renderer::get()->m_opts.enableAA);
    }
}

void DemoApp::uiDrawEntityGraph(Entity* entity) 
{
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    char* label = entity->m_name;
    if (m_selectedEntity && strcmp(entity->m_name, m_selectedEntity->m_name) == 0u)
    {
        baseFlags |= ImGuiTreeNodeFlags_Selected;
    }
    if (entity->m_child.size() > 0)
    {
        if (ImGui::TreeNodeEx(label, baseFlags))
        {
            for (auto* child : entity->m_child)
            {
                uiDrawEntityGraph(child);
            }
            ImGui::TreePop();
        }
    } else {
        ImGuiTreeNodeFlags nodeFlags = baseFlags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx(label, nodeFlags);
    }
}

void DemoApp::drawLightingWidgets()
{
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    auto sceneManager = SceneManager::get();
    // directional Lights
    if (ImGui::TreeNodeEx("Sun Light", baseFlags))
    {
        for (u32 i = 0; i < m_scenes[m_currentScene]->dLights.size(); ++i)
        {
            char nameBuf[50];
            sprintf_s(nameBuf, "Sun Light %d", i);
            if (ImGui::TreeNode(nameBuf))
            {
                ImGui::Text("Direction:");
                ImGui::SameLine();
                ImGui::InputFloat3("##Direction", &m_scenes[m_currentScene]->dLights[i].direction.x);
                ImGui::Text("Intensity:");
                ImGui::SameLine();
                ImGui::SliderFloat("##Intensity", &m_scenes[m_currentScene]->dLights[i].color.w, 0.f, 100.f, nullptr, 1.0f);
                ImGui::Text("Color:");
                ImGui::ColorPicker3("##Color", &m_scenes[m_currentScene]->dLights[i].color.r);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    // point lights
    if (ImGui::TreeNodeEx("Point Lights", baseFlags))
    {
        for (u32 i = 0; i < m_scenes[m_currentScene]->pointLights.size(); ++i)
        {
            char nameBuf[64];
            sprintf_s(nameBuf, "PointLight %d", i);
            if (ImGui::TreeNode(nameBuf))
            {
                ImGui::Text("Position:");
                ImGui::SameLine();
                ImGui::InputFloat3("##Position", &m_scenes[m_currentScene]->pointLights[i].position.x);
                ImGui::Text("Intensity:");
                ImGui::SliderFloat("##Intensity", &m_scenes[m_currentScene]->pointLights[i].color.w, 0.f, 100.f, nullptr, 1.0f);
                ImGui::Text("Color:");
                ImGui::ColorPicker3("##Color", &m_scenes[m_currentScene]->pointLights[i].color.r);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

void DemoApp::drawSceneViewport()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoCollapse;
    auto renderer = m_graphicsSystem->getRenderer();
    ImGui::SetNextWindowSize(ImVec2((f32)renderer->m_windowWidth, (f32)renderer->m_windowHeight));
    glm::vec2 viewportPos = gEngine->getSceneViewportPos();
    ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y));
    ImGui::SetNextWindowContentSize(ImVec2(renderer->m_windowWidth, renderer->m_windowHeight));
    // disable window padding for this window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    Cyan::UI::beginWindow("Scene Viewport", flags);
    {
        // fit the renderer viewport to ImGUi window size
        ImVec2 min = ImGui::GetWindowContentRegionMin();
        ImVec2 max = ImGui::GetWindowContentRegionMax();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 a(windowPos.x + min.x, windowPos.y + min.y);
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 b(windowPos.x + windowSize.x - 1, windowPos.y + windowSize.y - 1);

        // blit final render output texture to current ImGui window
        if (!activeDebugViewTexture)
        {
            activeDebugViewTexture = renderer->getColorOutTexture();
        }
        ImGui::Image(reinterpret_cast<void*>((intptr_t)activeDebugViewTexture->handle), ImVec2(max.x - min.x, max.y - min.y), ImVec2(0, 1), ImVec2(1, 0));

#if 0
        // TODO: refactor ray picking and gizmos 
        // TODO: when clicking on some objects (meshes) in the scene, the gizmo will be rendered not at the 
        // center of the mesh, this may seem incorrect at first glance but it's actually expected because some parts of the mesh
        // is offseted from the center of the mesh's transform. The gizmo is always rendered at the mesh's object space origin. In
        // some cases, the mesh may not overlap the origin or centered at the origin in object space in the first place, so it's expected
        // that the gizmo seems "detached" from the mesh itself.
        if (m_selectedEntity)
        {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
            ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

            glm::mat4 transformMatrix = m_selectedNode->getWorldTransform().toMatrix();
            glm::mat4 delta(1.0f);
            Camera& camera = m_scenes[m_currentScene]->getActiveCamera();
            if (ImGuizmo::Manipulate(&camera.view[0][0], 
                                &camera.projection[0][0],
                                ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, &transformMatrix[0][0], &delta[0][0]))
            {
                // apply the change in transform (delta) to local transform
                m_selectedNode->setLocalTransform(delta * m_selectedNode->getLocalTransform().toMatrix());
            }
        }
#endif
    }
    Cyan::UI::endWindow();
    ImGui::PopStyleVar();
}

void DemoApp::drawRenderSettings()
{
    auto renderer = m_graphicsSystem->getRenderer();
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    // ibl controls
    if (ImGui::TreeNodeEx("Lighting", baseFlags))
    {
        if (ImGui::TreeNodeEx("Direct lighting", baseFlags))
        {
            ImGui::Text("Diffuse");
            ImGui::SameLine();
            ImGui::SliderFloat("##Diffuse", &m_directDiffuseSlider, 0.f, 1.f, "%.2f");
            ImGui::Text("Specular");
            ImGui::SameLine();
            ImGui::SliderFloat("##Specular", &m_directSpecularSlider, 0.f, 1.f, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Indirect lighting", baseFlags))
        {
            ImGui::Text("Diffuse");
            ImGui::SameLine();
            ImGui::SliderFloat("##IndirectDiffuse", &m_indirectDiffuseSlider, 0.f, 20.f, "%.2f");
            ImGui::Text("Specular");
            ImGui::SameLine();
            ImGui::SliderFloat("##IndirectSpecular", &m_indirectSpecularSlider, 0.f, 20.f, "%.2f");
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Light Probes", baseFlags))
    {
        ImGui::Separator();
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Post-Processing", baseFlags))
    {
        // bloom settings
        ImGui::Text("Bloom");
        ImGui::SameLine();
        ImGui::Checkbox("##Enabled", &renderer->m_opts.enableBloom); 

        // exposure settings
        ImGui::Text("Exposure");
        ImGui::SameLine();
        ImGui::SliderFloat("##Exposure", &renderer->m_opts.exposure, 0.f, 10.f, "%.2f");
        ImGui::TreePop();
    }
}

void DemoApp::debugIrradianceCache()
{
    // auto pathTracer = Cyan::PathTracer::get();
    auto renderer = Cyan::Renderer::get();
    auto assetManager = Cyan::AssetManager::get();
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
    auto cubeMesh = assetManager->getAsset<Cyan::Mesh>("CubeMesh");
    auto circleMesh = assetManager->getAsset<Cyan::Mesh>("circle_mesh");
    auto debugShader = Cyan::getMaterialShader("DebugShadingShader");
    Camera& camera = m_scenes[m_currentScene]->getActiveCamera();
    glm::mat4 vp = camera.projection * camera.view;
    glm::vec4 color0(1.f, 1.f, 1.f, 1.f);
    glm::vec4 color1(0.f, 1.f, 0.f, 1.f);
    glm::vec4 color2(0.f, 0.f, 1.f, 1.f);

    auto debugDrawCube = [&](const glm::vec3& pos, const glm::vec3& scale, glm::vec4& color) {
        ctx->setShader(debugShader);
        ctx->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
        Transform transform;
        transform.m_translate = pos;
        transform.m_scale = scale;
        glm::mat4 mvp = vp * transform.toMatrix();
        debugShader->setUniformVec4("color", &color.r);
        for (u32 sm = 0; sm < cubeMesh->numSubmeshes(); ++sm)
        {
            debugShader->setUniformMat4("mvp", &mvp[0][0]);
            renderer->drawMesh(cubeMesh);
        }
    };

    auto debugDrawCircle = [&](const glm::vec3& pos, const glm::vec3& n, const glm::vec3& scale, glm::vec4& color) {
        ctx->setShader(debugShader);
        ctx->setPrimitiveType(Cyan::PrimitiveType::Line);
        glm::mat3 tangentFrame = Cyan::tangentToWorld(n);
        glm::mat4 rotation = { 
            glm::vec4(tangentFrame[0], 0.f),
            glm::vec4(tangentFrame[2], 0.f),
            glm::vec4(tangentFrame[1], 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f)
        };
        glm::mat4 m(1.f);
        m = glm::translate(m, pos);
        m *= rotation;
        m = glm::scale(m, glm::vec3(scale));
        glm::mat4 mvp = vp * m;
        debugShader->setUniformVec4("color", &color.r);
        for (u32 sm = 0; sm < circleMesh->numSubmeshes(); ++sm)
        {
            debugShader->setUniformMat4("mvp", &mvp[0][0]);
            renderer->drawMesh(circleMesh);
        }
    };

    glDisable(GL_CULL_FACE);
    {
#if 0
        // visualize irradiance records
        u32 start = pathTracer->m_irradianceCache->m_numRecords * .0f;
        u32 end = pathTracer->m_irradianceCache->m_numRecords * 1.f;
        for (u32 i = start; i < end; ++i)
        {
            auto& record = pathTracer->m_irradianceCache->m_records[i];
            debugDrawCircle(record.position, record.normal, glm::vec3(record.r), color0);
            debugDrawCube(record.position, glm::vec3(record.r * .02f), color1);
        }
#endif
#if 0
        // visualize translational gradients
        for (u32 i = 0; i < m_debugLines.size(); ++i)
        {
            m_debugLines[i].draw(vp);
        }
        // visualize octree
        for (u32 i = 0; i < pathTracer->m_debugData.octreeBoundingBoxes.size(); ++i)
        {
            pathTracer->m_debugData.octreeBoundingBoxes[i].draw(vp);
        }
#endif
    }
    glEnable(GL_CULL_FACE);
}

void DemoApp::render()
{
    // frame timer
    Cyan::Toolkit::GpuTimer frameTimer("render()");
    {
        auto renderer = m_graphicsSystem->getRenderer();
        // scene & debug objects
        renderer->render(m_scenes[m_currentScene], [this]() {
            debugIrradianceCache();
        });
        // ui
        renderer->renderUI([this]() {
            drawSceneViewport();
            drawDebugWindows();
        });
        // request flip
        renderer->flip();
    }
    frameTimer.end();
    m_lastFrameDurationInMs = frameTimer.m_durationInMs;
}

void DemoApp::shutDown()
{

    gEngine->finalize();
    delete gEngine;
}

void DemoApp::zoomCamera(Camera& camera, double dx, double dy)
{
    const f32 speed = 0.3f;
    // translate the camera along its lookAt direciton
    glm::vec3 translation = camera.forward * (float)(speed * dy);
    glm::vec3 v1 = glm::normalize(camera.position + translation - camera.lookAt); 
    // TODO: debug this
    if (glm::dot(v1, camera.forward) >= 0.f)
    {
        camera.position = camera.lookAt - 0.001f * camera.forward;
    }
    else
    {
        camera.position += translation;
    }
}

void DemoApp::orbitCamera(Camera& camera, double deltaX, double deltaY)
{
    // Camera& camera = m_scenes[m_currentScene]->getActiveCamera();
    // Note:(Min) limit the camera control to camera 0 for now
    // Camera& camera = m_scenes[m_currentScene]->cameras[0];

    /* Orbit around where the camera is looking at */
    float phi = deltaX * kCameraOrbitSpeed; 
    float theta = deltaY * kCameraOrbitSpeed;
    glm::vec3 p = camera.position - camera.lookAt;
    glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.up);
    quat = glm::rotate(quat, -theta, camera.right);
    glm::mat4 model(1.f);
    model = glm::translate(model, camera.lookAt);
    glm::mat4 rot = glm::toMat4(quat);
    glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
    camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
}

void DemoApp::rotateCamera(double deltaX, double deltaY)
{

}

void DemoApp::switchCamera()
{
    u32 cameraIdx = m_scenes[m_currentScene]->activeCamera;
    m_scenes[m_currentScene]->activeCamera = (cameraIdx + 1) % 2;
}