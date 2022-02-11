#include <iostream>
#include <queue>
#include <functional>
#include <stdlib.h>
#include <thread>

#include "PbrApp.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "ImGuizmo/ImGuizmo.h"
#include "stb_image.h"
#include "ArHosekSkyModel.h"

#include "CyanUI.h"
#include "CyanAPI.h"
#include "Light.h"
#include "Shader.h"
#include "Mesh.h"
#include "RenderPass.h"

#define REBAKE_LIGHTMAP 0
#define MOUSE_PICKING   1

/* Constants */
// In radians per pixel 
static float kCameraOrbitSpeed = 0.005f;
static float kCameraRotateSpeed = 0.005f;

struct HosekSkyLight
{
    ArHosekSkyModelState* stateR;
    ArHosekSkyModelState* stateG;
    ArHosekSkyModelState* stateB;

    HosekSkyLight(const glm::vec3& sunDir, const glm::vec3& groundAlbedo)
        : stateR(nullptr), stateG(nullptr), stateB(nullptr)
    {
        // in radians
        f32 solarElevation = acos(glm::dot(sunDir, glm::vec3(0.f, 1.f, 0.f)));
        auto stateR = arhosek_rgb_skymodelstate_alloc_init(2.f, groundAlbedo.r, solarElevation);
        auto stateG = arhosek_rgb_skymodelstate_alloc_init(2.f, groundAlbedo.g, solarElevation);
        auto stateB = arhosek_rgb_skymodelstate_alloc_init(2.f, groundAlbedo.b, solarElevation);
    }

    glm::vec3 sample(const glm::vec3& dir)
    {

    }
};

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

void PbrApp::dispatchCameraCommand(CameraCommand& command)
{
    PbrApp* app = PbrApp::get();
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

struct DebugRenderPass : Cyan::RenderPass
{
    DebugRenderPass(Cyan::RenderTarget* renderTarget, Cyan::Viewport viewport, PbrApp* app)
        : RenderPass(renderTarget, viewport), m_app(app)
    {

    }

    ~DebugRenderPass() { }

    virtual void render() override
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setRenderTarget(m_renderTarget);
        ctx->setViewport(m_viewport);
        m_app->debugRenderOctree();
    }

    PbrApp* m_app;
};

namespace Pbr
{
    void mouseCursorCallback(double cursorX, double cursorY, double deltaX, double deltaY)
    {
        PbrApp* app = PbrApp::get();
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
        PbrApp* app = PbrApp::get();
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
        PbrApp* app = PbrApp::get();
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
        PbrApp* app = PbrApp::get();
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

static PbrApp* gApp = nullptr;

PbrApp* PbrApp::get()
{
    if (gApp)
        return gApp;
    return nullptr;
}

PbrApp::PbrApp()
: m_debugRayTracingNormal(0.f, 1.f, 0.f)
, bOrbit(false)
{
    bOrbit = false;
    gApp = this;
    m_scenes.resize(Scenes::kCount, nullptr);
    activeDebugViewTexture = nullptr;
    m_currentDebugView = -1;
}

bool PbrApp::mouseOverUI()
{
    return (m_mouseCursorX < 400.f && m_mouseCursorX > 0.f && m_mouseCursorY < 720.f && m_mouseCursorY > 0.f);
}

Cyan::StandardPbrMaterial* PbrApp::createStandardPbrMatlInstance(Scene* scene, Cyan::PbrMaterialParam params, bool isStatic)
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

void PbrApp::createHelmetInstance(Scene* scene)
{
    auto textureManager = m_graphicsSystem->getTextureManager();
    auto sceneManager = SceneManager::getSingletonPtr();
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
}

void PbrApp::initDemoScene00()
{
    Cyan::Toolkit::GpuTimer timer("initDemoScene00()", true);
    auto textureManager = m_graphicsSystem->getTextureManager();
    auto sceneManager = SceneManager::getSingletonPtr();
    m_scenes[Scenes::Demo_Scene_00] = sceneManager->createScene("demo_scene_00", "C:\\dev\\cyanRenderEngine\\scene\\demo_scene_00.json");
    Scene* demoScene00 = m_scenes[Scenes::Demo_Scene_00];

    Cyan::PbrMaterialParam params = { };
    params.flatBaseColor = glm::vec4(1.f);
    params.kRoughness = .8f;
    params.kMetallic = .02f;
    Cyan::StandardPbrMaterial* defaultMatl = createStandardPbrMatlInstance(demoScene00, params, false);

    // helmet
    {
        createHelmetInstance(demoScene00);
        auto helmet = sceneManager->getEntity(demoScene00, "DamagedHelmet");
        auto helmetMesh = helmet->getSceneNode("HelmetMesh")->m_meshInstance->m_mesh;
        helmetMesh->m_bvh = new Cyan::MeshBVH(helmetMesh);
        helmetMesh->m_bvh->build();
    }
    // bunnies
    {
        Cyan::PbrMaterialParam params = { };
        Entity* bunny0 = sceneManager->getEntity(demoScene00, "Bunny0");
        Entity* bunny1 = sceneManager->getEntity(demoScene00, "Bunny1");
        params.flatBaseColor = glm::vec4(0.855, 0.647, 0.125f, 1.f);
        params.kRoughness = 0.1f;
        params.kMetallic = 1.0f;

        auto bunnyMatl = createStandardPbrMatlInstance(demoScene00, params, bunny0->m_static);
        bunny0->setMaterial("BunnyMesh", -1, bunnyMatl);
        bunny1->setMaterial("BunnyMesh", -1, bunnyMatl);
    }
    // man
    {
        Cyan::PbrMaterialParam params = { };
        Entity* man = sceneManager->getEntity(demoScene00, "Man");
        params.flatBaseColor = glm::vec4(0.855, 0.855, 0.855, 1.f);
        params.kRoughness = 0.3f;
        params.kMetallic = 0.3f;
        auto manMatl = createStandardPbrMatlInstance(demoScene00, params, man->m_static);
        man->setMaterial("ManMesh", -1, manMatl);
    }
    // lighting
    {
        glm::vec3 sunDir = glm::normalize(glm::vec3(1.0f, 0.5f, 1.8f));
        sceneManager->createDirectionalLight(demoScene00, glm::vec3(1.0f, 0.9, 0.7f), sunDir, 3.6f);
        // sky light
        auto sceneManager = SceneManager::getSingletonPtr();
        // todo: refactor lights, decouple them from Entity
        Entity* skyBoxEntity = sceneManager->createEntity(demoScene00, "SkyBox", Transform(), true);
        skyBoxEntity->m_sceneRoot->attach(sceneManager->createSceneNode(demoScene00, "CubeMesh", Transform(), Cyan::getMesh("CubeMesh"), false));
        skyBoxEntity->setMaterial("CubeMesh", 0, m_skyMatl);
        skyBoxEntity->m_selectable = false;
        skyBoxEntity->m_includeInGBufferPass = false;

        // light probes
        m_irradianceProbe = sceneManager->createIrradianceProbe(demoScene00, glm::vec3(0.f, 2.f, 0.f), glm::uvec2(512u, 512u), glm::uvec2(64u, 64u));
        m_reflectionProbe = sceneManager->createReflectionProbe(demoScene00, glm::vec3(0.f, 3.f, 0.f), glm::uvec2(2048u, 2048u));

        demoScene00->m_reflectionProbe = m_reflectionProbe;
        // procedural sky
        glm::vec3 groundAlbedo(1.f, 0.5f, 0.5f);
    }
    // grid of shader ball on the table
    {
        glm::vec3 gridLowerLeft(-2.1581, 1.1629, 2.5421);
        glm::vec3 defaultAlbedo(1.f);
        glm::vec3 silver(.753f, .753f, .753f);
        glm::vec3 gold(1.f, 0.843f, 0.f);
        glm::vec3 chrome(1.f);
        glm::vec3 plastic(0.061f, 0.757f, 0.800f);
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
                auto meshNode = sceneManager->createSceneNode(demoScene00, meshNodeName, transform, Cyan::getMesh("shaderball_mesh"));
                shaderBall->attachSceneNode(meshNode);
                auto matl = createStandardPbrMatlInstance(demoScene00, matlParams[j][i], shaderBall->m_static);
                shaderBall->setMaterial(meshNodeName, -1, defaultMatl);
                shaderBall->setMaterial(meshNodeName, 0, matl);
                shaderBall->setMaterial(meshNodeName, 1, matl);
                shaderBall->setMaterial(meshNodeName, 5, matl);
            }
        }
    }
    // room
    {
        Entity* room = sceneManager->getEntity(demoScene00, "Room");
        auto roomNode = room->getSceneNode("RoomMesh");
        auto planeNode = room->getSceneNode("PlaneMesh");
        Cyan::Mesh* roomMesh = roomNode->m_meshInstance->m_mesh;
        Cyan::Mesh* planeMesh = planeNode->m_meshInstance->m_mesh;
        roomMesh->m_bvh = new Cyan::MeshBVH(roomMesh);
        roomMesh->m_bvh->build();

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
        auto lightMapManager = Cyan::LightMapManager::getSingletonPtr();
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
        // todo: debug this, how is the material created for the room?
        // directly use prebaked lightmap
        auto lightMapManager = Cyan::LightMapManager::getSingletonPtr();
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
        auto planeMatl = createStandardPbrMatlInstance(demoScene00, params, room->m_static);
        room->setMaterial("PlaneMesh", -1, planeMatl);
        sceneManager->updateSceneGraph(demoScene00);
#endif
        // path tracing
        {
            // pathTraceScene(demoScene00);
        }
    }
    timer.end();
}

void PbrApp::pathTraceScene(Scene* scene)
{
    auto pathTracer = Cyan::PathTracer::getSingletonPtr();
    pathTracer->m_renderMode = Cyan::PathTracer::RenderMode::Render; pathTracer->setScene(scene);
    pathTracer->run(scene->getActiveCamera());

    // debug
    auto shader = Cyan::createShader("DebugShadingShader", "../../shader/debug_color_vs.glsl", "../../shader/debug_color_fs.glsl");
}

void PbrApp::initSponzaScene()
{
    Cyan::Toolkit::GpuTimer timer("initSponzaScene()", true);
    auto sceneManager = SceneManager::getSingletonPtr();
    m_scenes[Scenes::Sponza_Scene] = sceneManager->createScene("sponza_scene", "C:\\dev\\cyanRenderEngine\\scene\\sponza_scene.json");
    Scene* sponzaScene = m_scenes[Scenes::Sponza_Scene];
    m_currentScene = static_cast<u32>(Scenes::Sponza_Scene);

    Entity* sponza = sceneManager->getEntity(sponzaScene, "Sponza");
    auto sponzaNode = sponza->getSceneNode("SponzaMesh");
    Cyan::Mesh* sponzaMesh = sponzaNode->m_meshInstance->m_mesh;

    // lighting
    glm::vec3 sunDir = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
    sceneManager->createDirectionalLight(sponzaScene, glm::vec3(1.0f, 0.9, 0.7f), sunDir, 3.6f);
    // sky light
    Entity* skyBoxEntity = sceneManager->createEntity(sponzaScene, "SkyBox", Transform(), true);
    skyBoxEntity->m_sceneRoot->attach(sceneManager->createSceneNode(sponzaScene, "CubeMesh", Transform(), Cyan::getMesh("CubeMesh"), false));
    skyBoxEntity->setMaterial("CubeMesh", 0, m_skyMatl);
    skyBoxEntity->m_selectable = false;
    skyBoxEntity->m_includeInGBufferPass = false;
    // light probes
    m_irradianceProbe = sceneManager->createIrradianceProbe(sponzaScene, glm::vec3(0.f, 2.f, 0.f), glm::uvec2(512u, 512u), glm::uvec2(64u, 64u));
    m_reflectionProbe = sceneManager->createReflectionProbe(sponzaScene, glm::vec3(0.f, 3.f, 0.f), glm::uvec2(2048u, 2048u));
    sponzaScene->m_reflectionProbe = m_reflectionProbe;
    // procedural sky
    glm::vec3 groundAlbedo(1.f, 0.5f, 0.5f);

    // path tracing
#if 0
    {
        sponzaMesh->m_bvh = new Cyan::MeshBVH(sponzaMesh);
        sponzaMesh->m_bvh->build();
        pathTraceScene(sponzaScene);
    }
#endif
}

void PbrApp::initScenes()
{
    auto renderer = Cyan::Renderer::getSingletonPtr();
    glm::vec2 viewportSize = renderer->getViewportSize();
    float aspectRatio = viewportSize.x / viewportSize.y;
    auto initCamera = [&](Scene* scene)
    {
        for (auto& camera : scene->cameras)
        {
            camera.aspectRatio = aspectRatio;
            camera.projection = glm::perspective(glm::radians(camera.fov), aspectRatio, camera.n, camera.f);
        }
    };

#ifdef SCENE_SPONZA
    {
        initSponzaScene();
        initCamera(m_scenes[Scenes::Sponza_Scene]);
    }
#endif
#ifdef SCENE_DEMO_00
    {
        initDemoScene00();
        initCamera(m_scenes[Scenes::Demo_Scene_00]);
    }
#endif
}

void PbrApp::initShaders()
{
    Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs", "../../shader/shader_pbr.fs");
    m_skyBoxShader = Cyan::createShader("SkyBoxShader", "../../shader/shader_skybox.vs", "../../shader/shader_skybox.fs");
    m_skyShader = Cyan::createShader("SkyShader", "../../shader/shader_sky.vs", "../../shader/shader_sky.fs");
}

void PbrApp::initSkyBoxes()
{
    Cyan::Toolkit::GpuTimer timer("initEnvMaps()", true);
    // image-based-lighting
    Cyan::Toolkit::createDistantLightProbe("pisa", "../../asset/cubemaps/pisa.hdr", true);

    m_currentProbeIndex = 0u;
    Cyan::Texture* skyBoxTex = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
    m_skyBoxMatl = Cyan::createMaterial(m_skyBoxShader)->createInstance(); 
    m_skyBoxMatl->bindTexture("envmapSampler", skyBoxTex);
    m_skyMatl = Cyan::createMaterial(m_skyShader)->createInstance();
    
    // this is necessary as we are setting z component of
    // the cubemap mesh to 1.f
    glDepthFunc(GL_LEQUAL);
    timer.end();
}

void PbrApp::initUniforms()
{

}

void PbrApp::init(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize)
{
    Cyan::Toolkit::GpuTimer timer("init()", true);
    using Cyan::Material;
    using Cyan::Mesh;

    // init engine
    gEngine->init({ appWindowWidth, appWindowHeight }, sceneViewportPos, renderSize);
    bRunning = true;
    // setup input control
    gEngine->registerMouseCursorCallback(&Pbr::mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&Pbr::mouseButtonCallback);
    gEngine->registerMouseScrollWheelCallback(&Pbr::mouseScrollWheelCallback);
    gEngine->registerKeyCallback(&Pbr::keyCallback);

    m_graphicsSystem = Cyan::GraphicsSystem::getSingletonPtr();

    initShaders();
    initUniforms();
    initSkyBoxes();
    initScenes();

#ifdef SCENE_SPONZA
    m_currentScene = Scenes::Sponza_Scene;
#endif
#ifdef SCENE_DEMO_00
    m_currentScene = Scenes::Demo_Scene_00;
#endif

    glEnable(GL_LINE_SMOOTH);
    glLineWidth(4.f);

    // ui
    m_ui.init(gEngine->getWindow().mpWindow);

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
    m_debugRay.init();
    m_debugRay.setColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    auto renderer = Cyan::Renderer::getSingletonPtr();
    m_debugRay.setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
    m_debugProbeIndex = 13;
    m_debugRo = glm::vec3(-5.f, 2.43, 0.86);
    m_debugRd = glm::vec3(0.0f, -0.29f, 0.47f);

    // clear color
    Cyan::getCurrentGfxCtx()->setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.f));

    // font
    ImGuiIO& io = ImGui::GetIO();
    m_font = io.Fonts->AddFontFromFileTTF("C:\\dev\\cyanRenderEngine\\asset\\fonts\\Roboto-Medium.ttf", 16.f);
    timer.end();
}

void PbrApp::updateMaterialData(Cyan::StandardPbrMaterial* matl)
{
    // per material instance lighting params
    matl->m_materialInstance->set("directDiffuseScale", m_directDiffuseSlider);
    matl->m_materialInstance->set("directSpecularScale", m_directSpecularSlider);
    // global lighting parameters
    matl->m_materialInstance->set("gLighting.indirectDiffuseScale", m_indirectDiffuseSlider);
    matl->m_materialInstance->set("gLighting.indirectSpecularScale", m_indirectSpecularSlider);
}

void PbrApp::beginFrame()
{
    Cyan::getCurrentGfxCtx()->clear();
    Cyan::getCurrentGfxCtx()->setViewport({ 0, 0, static_cast<u32>(gEngine->getWindow().width), static_cast<u32>(gEngine->getWindow().height) });
}

void PbrApp::bakeLightProbes(Cyan::ReflectionProbe* probes, u32 numProbes)
{
    auto renderer = Cyan::Renderer::getSingletonPtr();
    // global sun light shadow pass
    renderer->addDirectionalShadowPass(m_scenes[m_currentScene], m_scenes[m_currentScene]->getActiveCamera(), 0);
    for (u32 i = 0; i < numProbes; ++i)
    {
        probes[i].bake();
    }
}

void PbrApp::doPrecomputeWork()
{
    // precomute thingy here
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        // update probe
        auto sceneManager = SceneManager::getSingletonPtr();
        sceneManager->setDistantLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
#ifdef SCENE_DEMO_00 
        auto renderer = Cyan::Renderer::getSingletonPtr();
        renderer->beginRender();
        renderer->addDirectionalShadowPass(m_scenes[m_currentScene], m_scenes[m_currentScene]->getActiveCamera(), 0);
        renderer->render(m_scenes[m_currentScene]);
        renderer->endRender();
        // updateScene(m_scenes[m_currentScene]);
        m_reflectionProbe->bake();
        // m_irradianceProbe->sampleRadiance();
        // m_irradianceProbe->computeIrradiance();
        // ctx->setRenderTarget(nullptr, 0u);
#endif
    }
}

void PbrApp::run()
{
    doPrecomputeWork();
    while (bRunning)
    {
        // tick
        update();
        // render
        beginFrame();
        render();
        endFrame();
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

RayCastInfo PbrApp::castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize)
{
    // convert mouse cursor pos to view space 
    Cyan::Viewport viewport = m_graphicsSystem->getRenderer()->getViewport();
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
    m_debugRay.setVerts(glm::vec3(0.f, 0.f, -0.2f), glm::vec3(rd * 20.0f));
    glm::mat4 viewInverse = glm::inverse(camera.view);
    m_debugRay.setModel(viewInverse);

    glm::vec3 ro = glm::vec3(0.f);

    RayCastInfo closestHit;

    // ray intersection test against all the entities in the scene to find the closest intersection
    for (auto entity : m_scenes[m_currentScene]->entities)
    {
        if (entity->m_visible && entity->m_selectable)
        {
            RayCastInfo traceInfo = entity->intersectRay(ro, rd, camera.view); 
            if (traceInfo.t > 0.f && traceInfo < closestHit)
            {
                closestHit = traceInfo;
                printf("Cast a ray from mouse that hits %s \n", traceInfo.m_node->m_name);
            }
        }
    }
    glm::vec3 worldHit = computeMouseHitWorldSpacePos(camera, rd, closestHit);
    printf("Mouse hit world at (%.2f, %.2f, %.2f) \n", worldHit.x, worldHit.y, worldHit.z);
    return closestHit;
}

void PbrApp::updateScene(Scene* scene)
{
    // update camera
    u32 camIdx = 0u;
    Camera& camera = m_scenes[m_currentScene]->cameras[camIdx];
    CameraManager::updateCamera(camera);
    // update material parameters
    for (auto matl : m_scenes[m_currentScene]->m_materials)
        updateMaterialData(matl);
    SceneManager::getSingletonPtr()->updateSceneGraph(m_scenes[m_currentScene]);
}

void PbrApp::update()
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

void PbrApp::drawEntityPanel()
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

void PbrApp::drawDebugWindows()
{
    auto renderer = m_graphicsSystem->getRenderer();
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setRenderTarget(nullptr);

    // configure window pos and size
    ImVec2 debugWindowSize(gEngine->getWindow().width - renderer->m_viewport.m_width, 
                           gEngine->getWindow().height);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(debugWindowSize);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    m_ui.beginWindow("Editor", windowFlags);
    ImGui::PushFont(m_font);
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
                    auto textureManager = Cyan::TextureManager::getSingletonPtr();
                    i32 currentItem = 0;
                    u32 numTextures = textureManager->getNumTextures();
                    std::vector<const char*> textureNames(numTextures);
                    std::vector<Cyan::Texture*>& textures = textureManager->s_textures;
                    for (u32 i = 0; i < numTextures; ++i)
                        textureNames[i] = textures[i]->m_name.c_str();
                    ImGui::Text("Textures");
                    ImGui::Text("Number of textures: %u", numTextures);
                    ImGui::ListBox("##Textures", &currentItem, textureNames.data(), textureNames.size());
                    ImGui::Separator();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Debug"))
            {
                auto renderer = Cyan::Renderer::getSingletonPtr();
                u32 numDebugViews = 0;
                auto buildDebugView = [&](const char* viewName, Cyan::Texture* texture, u32 width=320, u32 height=180, bool flip = true)
                {
                    ImGui::Text(viewName);
                    if (flip)
                        ImGui::Image((ImTextureID)texture->m_id, ImVec2(width, height), ImVec2(0, 1), ImVec2(1, 0));
                    else
                        ImGui::Image((ImTextureID)texture->m_id, ImVec2(width, height));
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
                buildDebugView("PathTracing", Cyan::PathTracer::getSingletonPtr()->getRenderOutput());
#ifdef SCENE_DEMO_00
                Entity* room = SceneManager::getSingletonPtr()->getSingletonPtr()->getEntity(m_scenes[Demo_Scene_00], "Room");
                SceneNode* sceneNode = room->getSceneNode("RoomMesh");
                buildDebugView("LightMap", sceneNode->m_meshInstance->m_lightMap->m_texAltas, 320u, 320u);
#endif
                // TODO: debug following three debug visualizations 
                buildDebugView("SceneDepth",  renderer->m_sceneDepthTextureSSAA);
                buildDebugView("SceneNormal", renderer->m_sceneNormalTextureSSAA);
                buildDebugView("SceneGTAO",   renderer->m_ssaoTexture);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::PopFont();
    m_ui.endWindow();
}

void PbrApp::drawStats()
{
    {
        ImGui::Text("Frame time:                   %.2f ms", m_lastFrameDurationInMs);
        ImGui::Text("Number of entities:           %d", m_scenes[m_currentScene]->entities.size());
        ImGui::Checkbox("Super Sampling 4x", &Cyan::Renderer::getSingletonPtr()->m_bSuperSampleAA);
    }
}

void PbrApp::uiDrawEntityGraph(Entity* entity) 
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

void PbrApp::drawLightingWidgets()
{
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    auto sceneManager = SceneManager::getSingletonPtr();
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
        for (u32 i = 0; i < m_scenes[m_currentScene]->pLights.size(); ++i)
        {
            char nameBuf[64];
            sprintf_s(nameBuf, "PointLight %d", i);
            if (ImGui::TreeNode(nameBuf))
            {
                ImGui::Text("Position:");
                ImGui::SameLine();
                ImGui::InputFloat3("##Position", &m_scenes[m_currentScene]->pLights[i].position.x);
                ImGui::Text("Intensity:");
                ImGui::SliderFloat("##Intensity", &m_scenes[m_currentScene]->pLights[i].color.w, 0.f, 100.f, nullptr, 1.0f);
                ImGui::Text("Color:");
                ImGui::ColorPicker3("##Color", &m_scenes[m_currentScene]->pLights[i].color.r);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    // sky light
    std::vector<const char*> envMaps;
    u32 numProbes = Cyan::getNumProbes();
    for (u32 index = 0u; index < numProbes; ++index)
        envMaps.push_back(Cyan::getProbe(index)->m_baseCubeMap->m_name.c_str());
    m_ui.comboBox(envMaps.data(), numProbes, "EnvMap", &m_currentProbeIndex);

    ImGui::SameLine();
    if (m_ui.button("Load"))
    {

    }
}

void PbrApp::drawSceneViewport()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    auto renderer = m_graphicsSystem->getRenderer();
    Cyan::Viewport viewport = renderer->getViewport();
    ImGui::SetNextWindowSize(ImVec2((f32)viewport.m_width, (f32)viewport.m_height));
    glm::vec2 viewportPos = gEngine->getSceneViewportPos();
    ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y));
    ImGui::SetNextWindowContentSize(ImVec2(viewport.m_width, viewport.m_height));
    // disable window padding for this window
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    m_ui.beginWindow("Scene Viewport", flags);
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
            activeDebugViewTexture = renderer->m_outputColorTexture;
        ImGui::GetForegroundDrawList()->AddImage(reinterpret_cast<void*>((intptr_t)activeDebugViewTexture->m_id), 
            a, b, ImVec2(0, 1), ImVec2(1, 0));

        // TODO: refactor this
        // ray picking
        if (bRayCast && !ImGuizmo::IsOver())
        {
            RayCastInfo hitInfo = castMouseRay(glm::vec2(a.x, a.y), glm::vec2(windowSize.x, windowSize.y - min.y));
            m_selectedEntity = hitInfo.m_node->m_owner;
            m_selectedNode = hitInfo.m_node;
            auto ctx = Cyan::getCurrentGfxCtx();
            ctx->setDepthControl(Cyan::DepthControl::kDisable);
            ctx->setRenderTarget(Cyan::Renderer::getSingletonPtr()->getRenderOutputRenderTarget(), 0u);
            // TODO: the line is not rendered at exactly where the mouse is currently clicking
            m_debugRay.draw();
            ctx->setRenderTarget(nullptr, 0u);
            ctx->setDepthControl(Cyan::DepthControl::kEnable);
        }

        // TODO: gizmos 
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
    }
    m_ui.endWindow();
    ImGui::PopStyleVar();
}

void PbrApp::drawRenderSettings()
{
    auto renderer = m_graphicsSystem->getRenderer();
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    // ibl controls
    if (ImGui::TreeNodeEx("IBL settings", baseFlags))
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
            ImGui::SliderFloat("##IndirectDiffuse", &m_indirectDiffuseSlider, 0.f, 1.f, "%.2f");
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
        ImGui::Checkbox("##Enabled", &renderer->m_bloom); 

        // exposure settings
        ImGui::Text("Exposure");
        ImGui::SameLine();
        ImGui::SliderFloat("##Exposure", &renderer->m_exposure, 0.f, 10.f, "%.2f");
        ImGui::TreePop();
    }
}

void PbrApp::buildFrame()
{
    // construct work for current frame
    Cyan::Renderer* renderer = Cyan::Renderer::getSingletonPtr();
    //renderer->addDirectionalShadowPass(m_scenes[m_currentScene], m_scenes[m_currentScene]->getActiveCamera(), 0);
    renderer->addScenePass(m_scenes[m_currentScene]);
    {
        void* memory = renderer->getAllocator().alloc(sizeof(DebugRenderPass));
        DebugRenderPass* debugPass = new (memory) DebugRenderPass(renderer->m_sceneColorRTSSAA, {0u, 0u, renderer->m_sceneColorRTSSAA->m_width, renderer->m_sceneColorRTSSAA->m_height }, this);
        renderer->addCustomPass(debugPass);
    }
    renderer->addPostProcessPasses();
}

void debugRenderCube()
{

}

void PbrApp::debugRenderOctree()
{
    auto pathTracer = Cyan::PathTracer::getSingletonPtr();
    auto renderer = Cyan::Renderer::getSingletonPtr();
    Cyan::Octree* octree = pathTracer->m_irradianceCache->m_octree;
    std::queue<Cyan::OctreeNode*> nodes;
    for (u32 i = 0; i < pathTracer->m_debugObjects.octreeBoundingBoxes.size(); ++i)
    {
        pathTracer->m_debugObjects.octreeBoundingBoxes[i].setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
        pathTracer->m_debugObjects.octreeBoundingBoxes[i].draw();
    }
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
    auto cubeMesh = Cyan::getMesh("CubeMesh");
    auto debugShader = Cyan::getShader("DebugShadingShader");
    Camera& camera = m_scenes[m_currentScene]->getActiveCamera();
    glm::mat4 vp = camera.projection * camera.view;
    glm::vec4 color(1.f, 0.f, 0.f, 1.f);

    auto debugRenderCube = [&](const glm::vec3& pos, const glm::vec3& scale, glm::vec4& color) {
        ctx->setShader(debugShader);
        Transform transform;
        transform.m_translate = pos;
        transform.m_scale = scale;
        glm::mat4 mvp = vp * transform.toMatrix();
        debugShader->setUniformVec4("color", &color.r);
        for (u32 sm = 0; sm < cubeMesh->m_subMeshes.size(); ++sm)
        {
            debugShader->setUniformMat4f("mvp", &mvp[0][0]);
            ctx->setVertexArray(cubeMesh->m_subMeshes[sm]->m_vertexArray);
            ctx->drawIndexAuto(cubeMesh->m_subMeshes[sm]->m_numVerts);
        }
    };

    glDisable(GL_CULL_FACE);
    {
#if 1
        for (u32 i = 0; i < pathTracer->m_irradianceCache->m_numRecords; ++i)
        {
            auto& record = pathTracer->m_irradianceCache->m_cache[i];
            debugRenderCube(record.position, glm::vec3(0.01f), color);
            //debugRenderCube(record.position, glm::vec3(record.r), color);
        }
#else
        glm::vec4 red(1.f, 0.f, 0.f, 1.f);
        debugRenderCube(pathTracer->m_debugPos0, glm::vec3(0.02f), red);
        glm::vec4 blue(0.f, 0.f, 1.f, 1.f);
        debugRenderCube(pathTracer->m_debugPos1, glm::vec3(0.02f), blue);
#endif
    }
    glEnable(GL_CULL_FACE);
}

void PbrApp::render()
{
    // frame timer
    Cyan::Toolkit::GpuTimer frameTimer("render()");

    // update probe
    SceneManager::getSingletonPtr()->setDistantLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
    auto renderer = m_graphicsSystem->getRenderer();
    renderer->beginRender();
    buildFrame();
    renderer->render(m_scenes[m_currentScene]);
    renderer->endRender();

    // ui
    m_ui.begin();
    drawSceneViewport();
    drawDebugWindows();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // flip
    renderer->endFrame();
    frameTimer.end();
    m_lastFrameDurationInMs = frameTimer.m_durationInMs;
}

void PbrApp::endFrame()
{

}

void PbrApp::shutDown()
{
    // ImGui clean up
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    gEngine->shutDown();
    delete gEngine;
}

void PbrApp::zoomCamera(Camera& camera, double dx, double dy)
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

void PbrApp::orbitCamera(Camera& camera, double deltaX, double deltaY)
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

void PbrApp::rotateCamera(double deltaX, double deltaY)
{

}

void PbrApp::switchCamera()
{
    u32 cameraIdx = m_scenes[m_currentScene]->activeCamera;
    m_scenes[m_currentScene]->activeCamera = (cameraIdx + 1) % 2;
}