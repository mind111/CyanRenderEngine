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

// TODO: coding this way prevents different control from happening at the same time. Command type should
// really be just setting different bits of type member variable.
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
                    if (!app->mouseOverUI())
                    {
                        app->bRayCast = true;
                    }
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

enum Scenes
{
    Demo_Scene_00 = 0,
    Factory_Scene,
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
    m_currentDebugView = 0;
}

bool PbrApp::mouseOverUI()
{
    return (m_mouseCursorX < 400.f && m_mouseCursorX > 0.f && m_mouseCursorY < 720.f && m_mouseCursorY > 0.f);
}

#if 0
Cyan::MaterialInstance* createDefaultRayTracingMatl(Scene* scene, Shader* shader, PbrMaterialInputs inputs)
{
    Cyan::MaterialInstance* matl = Cyan::createMaterial(shader)->createInstance();
    matl->bindTexture("diffuseMaps[0]", inputs.baseColor);
    if (inputs.m_normalMap)
    {
        matl->set("hasNormalMap", 1.0f);
        matl->bindTexture("normalMap", inputs.m_normalMap);
    }
    if (inputs.m_occlusion)
    {
        matl->set("hasAoMap", 1.0f);
        matl->bindTexture("aoMap", inputs.m_occlusion);
    }
    if (inputs.roughness)
    {
        matl->set("hasRoughnessMap", 1.0f);
        matl->bindTexture("roughnessMap", inputs.roughness);
        matl->bindTexture("metallicMap", inputs.m_metallicMap);
    }
    else if (inputs.m_metallicRoughnessMap) 
    {
        matl->set("hasMetallicRoughnessMap", 1.0f);
        matl->bindTexture("metallicRoughnessMap", inputs.m_metallicRoughnessMap);
    }
    else
    {
        matl->set("uniformRoughness", inputs.kRoughness);
        matl->set("uniformMetallic", inputs.kMetallic);
    }

    matl->set("kDiffuse", 1.0f);
    matl->set("kSpecular", 1.0f);
    matl->set("disneyReparam", 1.0f);
    return matl;
}
#endif

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
    m_scenes[Scenes::Demo_Scene_00] = Cyan::createScene("demo_scene_00", "C:\\dev\\cyanRenderEngine\\scene\\demo_scene_00.json");
    Scene* demoScene00 = m_scenes[Scenes::Demo_Scene_00];

    auto textureManager = m_graphicsSystem->getTextureManager();
    auto sceneManager = SceneManager::getSingletonPtr();
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
        Entity* envMapEntity = sceneManager->createEntity(demoScene00, "Envmap", Transform(), false);
        envMapEntity->m_sceneRoot->attach(Cyan::createSceneNode(demoScene00, "CubeMesh", Transform(), Cyan::getMesh("CubeMesh"), false));
        envMapEntity->setMaterial("CubeMesh", 0, m_skyMatl);
        envMapEntity->m_selectable = false;
        envMapEntity->m_includeInGBufferPass = false;

        // a irradiance probe
        m_irradianceProbe = sceneManager->createIrradianceProbe(demoScene00, glm::vec3(0.f, 2.f, 0.f));
        // a reflection probe
        m_reflectionProbe = sceneManager->createReflectionProbe(demoScene00, glm::vec3(0.f, 3.f, 0.f));
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
                auto meshNode = Cyan::createSceneNode(demoScene00, meshNodeName, transform, Cyan::getMesh("shaderball_mesh"));
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
        // directly use prebaked lightmap
        auto lightMapManager = Cyan::LightMapManager::getSingletonPtr();
        lightMapManager->createLightMapFromTexture(roomNode, textureManager->getTexture("RoomMesh_lightmap"));
        lightMapManager->createLightMapFromTexture(planeNode, textureManager->getTexture("PlaneMesh_lightmap"));
        for (u32 sm = 0; sm < roomMesh->numSubMeshes(); ++sm)
        {
            roomNode->m_meshInstance->m_matls[sm]->set("uniformRoughness", .8f);
            roomNode->m_meshInstance->m_matls[sm]->set("uniformMetallic", .02f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMaterialProps.hasBakedLighting", 1.f);
            roomNode->m_meshInstance->m_matls[sm]->set("indirectDiffuseScale", 0.f);
            roomNode->m_meshInstance->m_matls[sm]->set("indirectSpecularScale", 0.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMaterialProps.hasBakedLighting", 1.f);
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
#endif
    }
    timer.end();
}

void PbrApp::initFactoryScene()
{
    Cyan::Toolkit::GpuTimer timer("initFactoryScene()", true);
    m_scenes[Scenes::Factory_Scene] = Cyan::createScene("factory_scene", "../../scene/default_scene/scene_factory.json");
    timer.end();
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

#ifdef SCENE_FACTORY
    {
        initFactoryScene();
        initCamera(m_scenes[Scenes::Factory_Scene]);
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
    m_pbrShader = Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs" , "../../shader/shader_pbr.fs");
    m_envmapShader = Cyan::createShader("EnvMapShader", "../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
    m_skyShader = Cyan::createShader("SkyShader", "../../shader/shader_sky.vs", "../../shader/shader_sky.fs");
}

void PbrApp::initEnvMaps()
{
    Cyan::Toolkit::GpuTimer timer("initEnvMaps()", true);
    // image-based-lighting
    Cyan::Toolkit::createLightProbe("pisa", "../../asset/cubemaps/pisa.hdr",           true);
    // Cyan::Toolkit::createLightProbe("grace-new", "../../asset/cubemaps/grace-new.hdr", true);
    // Cyan::Toolkit::createLightProbe("glacier", "../../asset/cubemaps/glacier.hdr",     true);
    // Cyan::Toolkit::createLightProbe("ennis", "../../asset/cubemaps/ennis.hdr",         true);
    // Cyan::Toolkit::createLightProbe("doge2", "../../asset/cubemaps/doge2.hdr",         true);
    // Cyan::Toolkit::createLightProbe("studio", "../../asset/cubemaps/studio_01_4k.hdr",  true);
    // Cyan::Toolkit:: createLightProbe("fire-sky", "../../asset/cubemaps/the_sky_is_on_fire_4k.hdr",  true);

    m_currentProbeIndex = 0u;
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
    m_envmapMatl = Cyan::createMaterial(m_envmapShader)->createInstance(); 
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);
    m_skyMatl = Cyan::createMaterial(m_skyShader)->createInstance();
    
    // this is necessary as we are setting z component of
    // the cubemap mesh to 1.f
    glDepthFunc(GL_LEQUAL);
    timer.end();
}

void PbrApp::initUniforms()
{
    // create uniforms
    u_kDiffuse = Cyan::createUniform("kDiffuse", Uniform::Type::u_float);
    u_kSpecular = Cyan::createUniform("kSpecular", Uniform::Type::u_float);
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
    initEnvMaps();
    initScenes();
#ifdef SCENE_HELMET
    m_currentScene = Scenes::Helmet_Scene;
#endif
#ifdef SCENE_FACTORY
    m_currentScene = Scenes::Factory_Scene;
#endif
#ifdef SCENE_DEMO_00
    m_currentScene = Scenes::Demo_Scene_00;
#endif

    glEnable(GL_LINE_SMOOTH);
    glLineWidth(4.f);

    // ui
    m_ui.init(gEngine->getWindow().mpWindow);

    // misc
    m_roughness = 0.f;
    bRayCast = false;
    bPicking = false;
    m_selectedEntity = nullptr;
    entityOnFocusIdx = 0;
    Cyan::setUniform(u_kDiffuse, 1.0f);
    Cyan::setUniform(u_kSpecular, 1.0f);
    m_debugViewIndex = 0u;
    bDisneyReparam = true;
    m_directDiffuseSlider = 1.f;
    m_directSpecularSlider = 1.f;
    m_indirectDiffuseSlider = 1.f;
    m_indirectSpecularSlider = 4.0f;
    m_directLightingSlider = 1.f;
    m_indirectLightingSlider = 0.f;
    m_wrap = 0.1f;
    m_debugRay.init();
    m_debugRay.setColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    auto renderer = Cyan::Renderer::getSingletonPtr();
    m_debugRay.setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
    m_debugProbeIndex = 13;
    m_debugRo = glm::vec3(-5.f, 2.43, 0.86);
    m_debugRd = glm::vec3(0.0f, -0.29f, 0.47f);
    m_debugDrawSSAO = false;
    m_debugPathTracing = false;

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

void PbrApp::doPrecomputeWork()
{
    // precomute thingy here
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        beginFrame();
#if 0
        m_probeVolume->sampleScene();
#endif
#if 1
        // update probe
        SceneManager::getSingletonPtr()->setLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
        m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
        m_envmapMatl->bindTexture("envmapSampler", m_envmap);

        auto renderer = Cyan::Renderer::getSingletonPtr();
        renderer->addDirectionalShadowPass(m_scenes[m_currentScene], m_scenes[m_currentScene]->getActiveCamera(), 0);
        renderer->beginRender();
        renderer->render();
        renderer->endRender();
        // m_irradianceProbe->sampleRadiance();
        // m_irradianceProbe->computeIrradiance();
        m_reflectionProbe->sampleRadiance();
        m_reflectionProbe->prefilter();
#endif
        endFrame();
        ctx->setRenderTarget(nullptr, 0u);
    }
    // path tracing
    {
        auto pathTracer = Cyan::PathTracer::getSingletonPtr();
        // pathTracer->run(m_scenes[m_currentScene]->getActiveCamera());
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
                    ImGui::Text("Num of textures: %u", numTextures);
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
                            activeDebugViewTexture = nullptr;
                        else activeDebugViewTexture = texture;
                    }
                    ImGui::Separator();
                    numDebugViews++;
                };
                buildDebugView("PathTracing", Cyan::PathTracer::getSingletonPtr()->getRenderOutput());
                Entity* room = SceneManager::getSingletonPtr()->getSingletonPtr()->getEntity(m_scenes[Demo_Scene_00], "Room");
                SceneNode* sceneNode = room->getSceneNode("RoomMesh");
                buildDebugView("LightMap", sceneNode->m_meshInstance->m_lightMap->m_texAltas, 320u, 320u);
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
        ImGui::Checkbox("Visualize irradiance probes", &m_irradianceProbe->m_visible);
        ImGui::Checkbox("Visualize reflection probes", &m_reflectionProbe->m_visible);
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

struct RayTracingDebugPass : public Cyan::RenderPass
{
    RayTracingDebugPass(Cyan::RenderTarget* renderTarget, Cyan::Viewport viewport, PbrApp* app)
        : RenderPass(renderTarget, viewport), m_app(app)
    {
        if (!m_debugWorldRay[0])
        {
            for (u32 i = 0; i < kNumDebugLines; i++)
            {
                m_debugWorldRay[i] = new Line();
                m_debugWorldRay[i]->init();
                m_debugWorldRay[i]->setColor(glm::vec4(1.f, 0.f, 0.f, 1.f));
            }
            for (u32 i = 0; i < 10; i++)
            {
                m_debugBoundryRay[i] = new Line();
                m_debugBoundryRay[i]->init();
                m_debugBoundryRay[i]->setColor(glm::vec4(0.f, 0.f, 1.f, 1.f));
            }
            for (u32 i = 0; i < kNumDebugLines; i++)
            {
                m_debugRaySegments[i] = new Line2D(glm::vec3(0.f), glm::vec3(0.f));
            }

            m_debugProbeLine = new Line();
            m_debugProbeLine->init();
            m_debugProbeLine->setColor(glm::vec4(0.f, 1.f, 0.f, 1.f));

            m_debugProbeLineBuffer = Cyan::createRegularBuffer(sizeof(Line) * 10);
        }
    }

    virtual void render() override
    {
        #if 0
        auto renderer = Cyan::Renderer::getSingletonPtr();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        {
            // f32* data = reinterpret_cast<f32*>(glMapNamedBuffer(m_app->m_debugRayBoundryBuffer->m_ssbo, GL_READ_WRITE));
            // char* dataCpy = (char*)data;
            // f32 numBoundryPoints = *data;
            // data += 4;
            // for (u32 i = 0; i < numBoundryPoints; ++i)
            // {
            //     glm::vec4 vert0;
            //     glm::vec4 vert1;
            //     memcpy(&vert0.x, data, sizeof(glm::vec4));
            //     memcpy(&vert1.x, data + 4, sizeof(glm::vec4));
            //     glm::vec3 v0Vec3 = glm::vec3(vert0.x, vert0.y, vert0.z);
            //     glm::vec3 v1Vec3 = glm::vec3(vert1.x, vert1.y, vert1.z);
            //     m_debugBoundryRay[i]->setVerts(v0Vec3, v1Vec3);
            //     glm::mat4 model(1.f);
            //     m_debugBoundryRay[i]->setModel(model);
            //     m_debugBoundryRay[i]->setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
            //     m_debugBoundryRay[i]->draw();
            //     data += 8;
            // }
            // memset(dataCpy, 0x0, m_app->m_debugRayBoundryBuffer->m_totalSize);
            // glUnmapNamedBuffer(m_app->m_debugRayBoundryBuffer->m_ssbo);
        }

        // line segment world ray
        {
            f32* data = reinterpret_cast<f32*>(glMapNamedBuffer(m_app->m_debugRayWorldBuffer->m_ssbo, GL_READ_WRITE));
            char* dataCopy = (char*)data;
            f32 numSegments = *data;
            // skip padding
            data += 4;
            for (u32 i = 0; i < numSegments; ++i)
            {
                glm::vec4 vert0;
                glm::vec4 vert1;
                memcpy(&vert0.x, data, sizeof(glm::vec4));
                memcpy(&vert1.x, data + 4, sizeof(glm::vec4));
                glm::vec3 v0Vec3 = glm::vec3(vert0.x, vert0.y, vert0.z);
                glm::vec3 v1Vec3 = glm::vec3(vert1.x, vert1.y, vert1.z);
                int probeIndex = vert0.w;
                glm::vec4 colors[6] = { 
                    glm::vec4(1.f, 0.f, 0.f, 1.f),
                    glm::vec4(0.f, 1.f, 0.f, 1.f),
                    glm::vec4(0.f, 0.f, 1.f, 1.f),
                    glm::vec4(1.f, 1.f, 0.f, 1.f),
                    glm::vec4(0.f, 1.f, 1.f, 1.f),
                    glm::vec4(1.f, 0.f, 1.f, 1.f)
                };
                m_debugWorldRay[i]->setColor(colors[probeIndex % 6]);
                m_debugWorldRay[i]->setVerts(v0Vec3, v1Vec3);
                glm::mat4 model(1.f);
                m_debugWorldRay[i]->setModel(model);
                m_debugWorldRay[i]->setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
                m_debugWorldRay[i]->draw();
                data += 8;
            }
            memset(dataCopy, 0x0, m_app->m_debugRayWorldBuffer->m_totalSize);
            glUnmapNamedBuffer(m_app->m_debugRayWorldBuffer->m_ssbo);
        }

        {
            f32* data = reinterpret_cast<f32*>(glMapNamedBuffer(m_debugProbeLineBuffer->m_ssbo, GL_READ_WRITE));
            char* dataCopy = (char*)data;
            f32 numSegments = *data;
            // skip padding
            data += 4;
            for (u32 i = 0; i < numSegments; ++i)
            {
                glm::vec4 vert0;
                glm::vec4 vert1;
                memcpy(&vert0.x, data, sizeof(glm::vec4));
                memcpy(&vert1.x, data + 4, sizeof(glm::vec4));
                glm::vec3 v0Vec3 = glm::vec3(vert0.x, vert0.y, vert0.z);
                glm::vec3 v1Vec3 = glm::vec3(vert1.x, vert1.y, vert1.z);
                m_debugProbeLine->setVerts(v0Vec3, v1Vec3);
                glm::mat4 model(1.f);
                m_debugProbeLine->setModel(model);
                m_debugProbeLine->setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
                m_debugProbeLine->draw();
                data += 8;
            }
            memset(dataCopy, 0x0, m_debugProbeLineBuffer->m_totalSize);
            glUnmapNamedBuffer(m_debugProbeLineBuffer->m_ssbo);
        }

        // draw projected debug ray on top of the octmap
        {
            f32* data = reinterpret_cast<f32*>(glMapNamedBuffer(m_app->m_debugRayOctBuffer->m_ssbo, GL_READ_WRITE));
            char* dataCopy = (char*)data;
            glm::vec2 vp0;
            glm::vec2 vp1;
            i32 numBoundryPoints = *reinterpret_cast<i32*>(data);
            // +2 to skipp paddings bytes
            data += 2;
            auto ctx = Cyan::getCurrentGfxCtx();
            ctx->setRenderTarget(m_renderTarget, 0u);
            ctx->setDepthControl(Cyan::DepthControl::kDisable);
            ctx->setViewport(m_viewport);
            // TODO: why the line data is not updated in real-time?
            for (i32 i = 0; i < i32(numBoundryPoints); ++i)
            {
                memcpy(&vp0.x, data, sizeof(glm::vec2));
                memcpy(&vp1.x, data + 2, sizeof(glm::vec2));
                data += 4;
                m_debugRaySegments[i]->setVerts(glm::vec3(vp0, 0.f), glm::vec3(vp1, 0.f));
                m_debugRaySegments[i]->setColor(glm::vec4(0.2f, 0.8f, .2f, 1.f));
                m_debugRaySegments[i]->draw();
            }
            ctx->setDepthControl(Cyan::DepthControl::kEnable);
            memset(dataCopy, 0x0, m_app->m_debugRayOctBuffer->m_totalSize);
            glUnmapNamedBuffer(m_app->m_debugRayOctBuffer->m_ssbo);
        }
        #endif
    }

    static const u32 kNumDebugLines = 102;
    static Line* m_debugWorldRay[kNumDebugLines];
    static Line* m_debugBoundryRay[10];
    static Line* m_debugProbeLine;
    static Line2D* m_debugRaySegments[kNumDebugLines];
    static RegularBuffer* m_debugProbeLineBuffer;
    PbrApp* m_app;
};

Line* RayTracingDebugPass::m_debugWorldRay[RayTracingDebugPass::kNumDebugLines] = { };
Line* RayTracingDebugPass::m_debugBoundryRay[10] = { };
Line2D* RayTracingDebugPass::m_debugRaySegments[RayTracingDebugPass::kNumDebugLines] = { };
Line* RayTracingDebugPass::m_debugProbeLine = nullptr;
RegularBuffer* RayTracingDebugPass::m_debugProbeLineBuffer = nullptr;

struct DebugAABBPass : public Cyan::RenderPass
{
    DebugAABBPass(Cyan::RenderTarget* renderTarget, Cyan::Viewport viewport, Scene* scene)
        : RenderPass(renderTarget, viewport), m_scene(scene)
    {

    }

    virtual void render() override
    {
        auto renderer = Cyan::Renderer::getSingletonPtr();
        for (auto entity : m_scene->entities)
        {
            std::queue<SceneNode*> queue;
            queue.push(entity->m_sceneRoot);
            while (!queue.empty())
            {
                auto node = queue.front();
                queue.pop();
                if (node->m_meshInstance)
                {
                    if (!node->m_hasAABB)
                    {
                        continue;
                    }
                    BoundingBox3f aabb = node->m_meshInstance->getAABB();
                    if (aabb.isValid)
                    {
                        aabb.setModel(node->m_worldTransform.toMatrix());
                        aabb.setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
                        aabb.draw();
                    }
                }
                for (auto child : node->m_child)
                {
                    queue.push(child);
                }
            }
        }
    }

    Scene* m_scene;
};

void PbrApp::buildFrame()
{
    // construct work for current frame
    Cyan::Renderer* renderer = Cyan::Renderer::getSingletonPtr();
    renderer->addDirectionalShadowPass(m_scenes[m_currentScene], m_scenes[m_currentScene]->getActiveCamera(), 0);
    renderer->addScenePass(m_scenes[m_currentScene]);
    renderer->addPostProcessPasses();
#if DEBUG_PROBE_TRACING
    {
        void* preallocated = renderer->getAllocator().alloc(sizeof(RayTracingDebugPass));
        auto renderTarget = m_probeVolume->m_probes[22]->m_octMapRenderTarget;
        RayTracingDebugPass* pass = new (preallocated) RayTracingDebugPass(renderTarget, Cyan::Viewport{0u,0u, renderTarget->m_width, renderTarget->m_height}, this);
        renderer->addCustomPass(pass);
    }
#endif
#if DRAW_DEBUG
    {
        void* preallocated = renderer->getAllocator().alloc(sizeof(DebugAABBPass));
        Cyan::RenderTarget* sceneRenderTarget = renderer->getSceneColorRenderTarget();
        DebugAABBPass* pass = new (preallocated) DebugAABBPass(sceneRenderTarget, Cyan::Viewport{0u,0u, sceneRenderTarget->m_width, sceneRenderTarget->m_height}, m_scenes[m_currentScene]);
        renderer->addCustomPass(pass);
    }
#endif
}

// todo: read about probe selection & multi-probe tracing
// todo: irradiance probe with visibility (prefiltered radial depth map)
void PbrApp::render()
{
    // frame timer
    Cyan::Toolkit::GpuTimer frameTimer("render()");
    auto renderer = m_graphicsSystem->getRenderer();

    // update probe
    SceneManager::getSingletonPtr()->setLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);

#if 0
    m_rayTracingMatl->bindBuffer("debugOctRayData", m_debugRayOctBuffer);
    m_rayTracingMatl->bindBuffer("debugWorldRayData", m_debugRayWorldBuffer);
    m_rayTracingMatl->bindBuffer("debugBoundryData", m_debugRayBoundryBuffer);
    m_rayTracingMatl->bindBuffer("debugProbeData", RayTracingDebugPass::m_debugProbeLineBuffer);

    m_rayTracingMatl->bindTexture("octRadiance", m_probeVolume->m_octRadianceGrid);
    m_rayTracingMatl->bindTexture("octNormal", m_probeVolume->m_octNormalGrid);
    m_rayTracingMatl->bindTexture("octRadialDepth", m_probeVolume->m_octRadialDepthGrid);
    m_rayTracingMatl->set("gProbeVolume.volumeDimension", &m_probeVolume->m_volumeDimension.x);
    m_rayTracingMatl->set("gProbeVolume.probeSpacing", &m_probeVolume->m_probeSpacing.x);
    m_rayTracingMatl->set("gProbeVolume.lowerLeftCorner", &m_probeVolume->m_lowerLeftCorner.x);
    m_rayTracingMatl->set("enableReflection", 1.f);
    m_rayTracingMatl->set("debugRo", &m_debugRo.x);
    m_rayTracingMatl->set("debugRd", &m_debugRd.x);
    // debug
    m_rayTracingMatl->set("debugTraceNormal", &m_debugRayTracingNormal.x);
    m_rayTracingMatl->set("debugProbeIndex", &m_debugProbeIndex);
    auto camera = m_scenes[m_currentScene]->getActiveCamera();
    m_rayTracingMatl->set("debugCameraPos", &camera.position.x);
    // TODO: cleanup! this is temporary, atomic counter is really similar to SSBO
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_debugRayAtomicCounter);
    u32* rayCounterPtr = (u32*)glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_READ_WRITE);
    memset(rayCounterPtr, 0x0, sizeof(u32));
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, m_debugRayAtomicCounter);
#endif
    renderer->beginRender();
    buildFrame();
    // m_reflectionProbe->sampleRadiance();
    // m_reflectionProbe->prefilter();
    renderer->render();
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