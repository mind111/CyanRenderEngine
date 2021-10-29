#include <iostream>
#include <queue>
#include <functional>
#include <stdlib.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include "stb_image.h"

#include "CyanUI.h"
#include "CyanAPI.h"
#include "Light.h"
#include "PbrApp.h"
#include "Shader.h"
#include "Mesh.h"
#include "RenderPass.h"

/*
    * particle system; particle rendering
    * grass rendering
    * procedural sky & clouds
    * look into ue4's skylight implementation
    * toon shading
    * normalized blinn phong shading
    * animation
    * select entity via ui; highlight selected entities in the scene
    * saving the scene and assets as binaries (serialization)
    * how to structure code around render pass

    * struct RenderPass
    * {
    *   Shader* m_shader;
    *   RenderTarget* m_renderTarget;
    *   VertexArray*  m_vertexArray;
    *   Material* (ShaderInputs) m_matl;
    * }
*/


/* Constants */
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

static PbrApp* gApp = nullptr;

PbrApp* PbrApp::get()
{
    if (gApp)
        return gApp;
    return nullptr;
}

PbrApp::PbrApp()
: m_debugRayTracingNormal(0.f, 1.f, 0.f)
{
    bOrbit = false;
    gApp = this;
}

bool PbrApp::mouseOverUI()
{
    return (m_mouseCursorX < 400.f && m_mouseCursorX > 0.f && m_mouseCursorY < 720.f && m_mouseCursorY > 0.f);
}

struct PbrMaterialInputs
{
    Cyan::Texture* m_baseColor;
    Cyan::Texture* m_roughnessMap;
    Cyan::Texture* m_metallicMap;
    Cyan::Texture* m_metallicRoughnessMap;
    Cyan::Texture* m_normalMap;
    Cyan::Texture* m_occlusion;
    float m_uRoughness;
    float m_uMetallic;
};

Cyan::MaterialInstance* createDefaultRayTracingMatl(Scene* scene, Shader* shader, PbrMaterialInputs inputs)
{
    Cyan::MaterialInstance* matl = Cyan::createMaterial(shader)->createInstance();
    matl->bindTexture("diffuseMaps[0]", inputs.m_baseColor);
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
    if (inputs.m_roughnessMap)
    {
        matl->set("hasRoughnessMap", 1.0f);
        matl->bindTexture("roughnessMap", inputs.m_roughnessMap);
        matl->bindTexture("metallicMap", inputs.m_metallicMap);
    }
    else if (inputs.m_metallicRoughnessMap) 
    {
        matl->set("hasMetallicRoughnessMap", 1.0f);
        matl->bindTexture("metallicRoughnessMap", inputs.m_metallicRoughnessMap);
    }
    else
    {
        matl->set("uniformRoughness", inputs.m_uRoughness);
        matl->set("uniformMetallic", inputs.m_uMetallic);
    }

    matl->bindBuffer("dirLightsData", scene->m_dirLightsBuffer);
    matl->bindBuffer("pointLightsData", scene->m_pointLightsBuffer);
    matl->set("kDiffuse", 1.0f);
    matl->set("kSpecular", 1.0f);
    matl->set("disneyReparam", 1.0f);
    return matl;
}

Cyan::MaterialInstance* createDefaultPbrMatlInstance(Scene* scene, PbrMaterialInputs inputs)
{
    Shader* pbrShader = Cyan::createShader("PbrShader", nullptr, nullptr);
    Cyan::MaterialInstance* matl = Cyan::createMaterial(pbrShader)->createInstance();
    matl->bindTexture("diffuseMaps[0]", inputs.m_baseColor);
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
    if (inputs.m_roughnessMap)
    {
        matl->set("hasRoughnessMap", 1.0f);
        matl->bindTexture("roughnessMap", inputs.m_roughnessMap);
        matl->bindTexture("metallicMap", inputs.m_metallicMap);
    }
    else if (inputs.m_metallicRoughnessMap) 
    {
        matl->set("hasMetallicRoughnessMap", 1.0f);
        matl->bindTexture("metallicRoughnessMap", inputs.m_metallicRoughnessMap);
    }
    else
    {
        matl->set("uniformRoughness", inputs.m_uRoughness);
        matl->set("uniformMetallic", inputs.m_uMetallic);
    }

    matl->bindBuffer("dirLightsData", scene->m_dirLightsBuffer);
    matl->bindBuffer("pointLightsData", scene->m_pointLightsBuffer);
    matl->set("kDiffuse", 1.0f);
    matl->set("kSpecular", 1.0f);
    matl->set("disneyReparam", 1.0f);
    return matl;
}

void PbrApp::initHelmetScene()
{
    Cyan::Toolkit::ScopedTimer timer("initHelmetScene()", true);
    // setup scenes
    Cyan::Toolkit::ScopedTimer loadSceneTimer("createScene()", true);
    Scene* helmetScene = Cyan::createScene("helmet_scene", "../../scene/default_scene/scene_config.json");
    auto renderer = Cyan::Renderer::getSingletonPtr();
    glm::vec2 viewportSize = renderer->getViewportSize();
    float aspectRatio = viewportSize.x / viewportSize.y;
    for (auto& camera : helmetScene->cameras)
    {
        // TODO: add setters
        camera.aspectRatio = aspectRatio;
        camera.projection = glm::perspective(glm::radians(camera.fov), aspectRatio, camera.n, camera.f);
    }
    auto sceneManager = SceneManager::getSingletonPtr();
    Entity* envMapEntity = sceneManager->createEntity(helmetScene, "Envmap", Transform());
    envMapEntity->m_sceneRoot->attach(Cyan::createSceneNode("CubeMesh", Transform(), Cyan::getMesh("CubeMesh"), false));
    envMapEntity->setMaterial("CubeMesh", 0, m_skyMatl);

    // additional spheres for testing lighting
    {
        PbrMaterialInputs inputs = { 0 };
        Entity* sphereEntity = sceneManager->getEntity(helmetScene, "Sphere0");
        Cyan::Texture* sphereAlbedo = Cyan::Toolkit::createFlatColorTexture("sphere_albedo", 1024u, 1024u, glm::vec4(0.8f, 0.8f, 0.6f, 1.f));
        inputs.m_baseColor = sphereAlbedo;
        inputs.m_uRoughness = 0.8f;
        inputs.m_uMetallic = 0.1f;
        m_sphereMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
        sphereEntity->setMaterial("SphereMesh", 0, m_sphereMatl);
    }
    timer.end();

    // open room
    {
        PbrMaterialInputs inputs = { };
        Entity* floor = sceneManager->getEntity(helmetScene, "Floor");
        Cyan::Texture* roomAlbedo = Cyan::Toolkit::createFlatColorTexture("RoomAlbedo", 64u, 64u, glm::vec4(1.00f, 0.90, 0.80, 1.f));
        Cyan::Texture* floorAlbedo = Cyan::Toolkit::createFlatColorTexture("FloorAlbedo", 64u, 64u, glm::vec4(1.00f, 0.50, 0.40, 1.f));
        inputs.m_baseColor = roomAlbedo;
        inputs.m_uRoughness = 0.8f;
        inputs.m_uMetallic = 0.1f;
        m_roomMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
        inputs.m_baseColor = floorAlbedo;
        m_floorMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
        floor->setMaterial("CubeMesh", 0, m_floorMatl);
        Entity* frontWall = sceneManager->getEntity(helmetScene, "Wall_0");
        frontWall->setMaterial("CubeMesh", 0, m_roomMatl);
        Entity* sideWall = sceneManager->getEntity(helmetScene, "Wall_1");
        sideWall->setMaterial("CubeMesh", 0, m_roomMatl);
    }

    auto textureManager = m_graphicsSystem->getTextureManager();
    m_helmetMatl = Cyan::createMaterial(m_pbrShader)->createInstance();
    // TODO: create a .cyanmatl file for defining materials?
    m_helmetMatl->bindTexture("diffuseMaps[0]", textureManager->getTexture("helmet_diffuse"));
    m_helmetMatl->bindTexture("normalMap", textureManager->getTexture("helmet_nm"));
    m_helmetMatl->bindTexture("metallicRoughnessMap", textureManager->getTexture("helmet_roughness"));
    m_helmetMatl->bindTexture("aoMap", textureManager->getTexture("helmet_ao"));
    m_helmetMatl->bindTexture("envmap", m_envmap);
    m_helmetMatl->bindBuffer("dirLightsData", helmetScene->m_dirLightsBuffer);
    m_helmetMatl->bindBuffer("pointLightsData", helmetScene->m_pointLightsBuffer);
    m_helmetMatl->set("hasAoMap", 1.f);
    m_helmetMatl->set("hasNormalMap", 1.f);
    m_helmetMatl->set("kDiffuse", 1.0f);
    m_helmetMatl->set("kSpecular", 1.0f);
    m_helmetMatl->set("hasMetallicRoughnessMap", 1.f);
    m_helmetMatl->set("disneyReparam", 1.f);
    Entity* helmet = sceneManager->getEntity(helmetScene, "DamagedHelmet");
    helmet->setMaterial("HelmetMesh", 0, m_helmetMatl);

    // cube
    // TODO: default material parameters
    {
        PbrMaterialInputs inputs = { };
        Entity* cube = sceneManager->getEntity(helmetScene, "Cube");
        Cyan::Texture* cubeAlbedo = Cyan::Toolkit::createFlatColorTexture("CubeAlbedo", 128, 128, glm::vec4(0.6, 0.6, 0.6, 1.0));
        inputs.m_baseColor = cubeAlbedo;
        inputs.m_uRoughness = 0.8f;
        inputs.m_uMetallic = 0.0f;
        m_cubeMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
        cube->setMaterial("UvCubeMesh", 0, m_cubeMatl);
    }
    // cone
    {
        PbrMaterialInputs inputs = { };
        Cyan::Texture* coneAlbedo = Cyan::Toolkit::createFlatColorTexture("ConeAlbedo", 64, 64, glm::vec4(1.0, 0.8, 0.8, 1.0));
        inputs.m_baseColor = coneAlbedo;
        inputs.m_uRoughness = 0.2f;
        inputs.m_uMetallic = 0.1f;
        Entity* cone = sceneManager->getEntity(helmetScene, "Cone");
        m_coneMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
        cone->setMaterial("ConeMesh", 0, m_coneMatl);
    }
    // cornell_box
    {
        PbrMaterialInputs inputs = { };
        Cyan::Texture* cornellAlbedo = Cyan::Toolkit::createFlatColorTexture("CornellAlbedo", 4, 4, glm::vec4(1.0, 0.9, 0.9, 1.0));
        inputs.m_baseColor = cornellAlbedo;
        inputs.m_uRoughness = 0.8f;
        inputs.m_uMetallic = 0.1f;
        Entity* cornellBox = sceneManager->getEntity(helmetScene, "CornellBox");
        m_cornellMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
        m_rayTracingShader = Cyan::createShader("RayTracingShader", "../../shader/shader_ray_tracing.vs", "../../shader/shader_ray_tracing.fs");
        m_rayTracingMatl = createDefaultRayTracingMatl(helmetScene, m_rayTracingShader, inputs);
        m_debugRayOctBuffer = Cyan::createRegularBuffer(sizeof(glm::vec2) * 11);
        m_debugRayWorldBuffer = Cyan::createRegularBuffer(sizeof(glm::vec4) * 300);
        m_debugRayBoundryBuffer = Cyan::createRegularBuffer(sizeof(glm::vec4) * 20);
        cornellBox->setMaterial("CornellBox", 0, m_rayTracingMatl);
    }

    // add lights into the scene
    sceneManager->createPointLight(helmetScene, glm::vec3(0.95, 0.59f, 0.149f), glm::vec3(-3.0f, 1.2f, 1.5f), 17.0f);
    sceneManager->createPointLight(helmetScene, glm::vec3(0.0f, 0.21f, 1.0f), glm::vec3(3.0f, 0.8f, -0.4f), 20.f);
    // top light
    sceneManager->createDirectionalLight(helmetScene, glm::vec3(1.0f, 1.0, 1.0f), glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)), 1.2f);

    // test light field probe
    { 
        glCreateBuffers(1, &m_debugRayAtomicCounter);
        glNamedBufferData(m_debugRayAtomicCounter, sizeof(u32), nullptr, GL_STATIC_DRAW);
        // m_lightFieldProbe = sceneManager->createLightFieldProbe(helmetScene, glm::vec3(-5.f, 1.155f, 0.f));
        m_probeVolume = sceneManager->createLightFieldProbeVolume(helmetScene, glm::vec3(-5.f, 1.155f, 0.f), glm::vec3(4.0f), glm::vec3(2.f));
        // m_irradianceProbe = sceneManager->createIrradianceProbe(helmetScene, glm::vec3(0.f));
    }
    /*
    // create more point lights
    {
        f32 spacing = 2.f;
        glm::vec3 translate = -glm::vec3(6.f, 0.f, -6.f);
        for (i32 r = 0; r < 4; ++r)
        {
            for (i32 col = 0; col < 4; ++ col)
            {
                glm::vec3 position(col * 4.0f, -0.5f, -r * 4.0f);
                position += translate;
                // randomize color ..?
                f32 red = static_cast<f32>((rand() % 256)) / 255.f;
                f32 green = static_cast<f32>((rand() % 256)) / 255.f;
                f32 blue = static_cast<f32>((rand() % 256)) / 255.f;
                sceneManager->createPointLight(helmetScene, glm::vec3(red, green, blue), position, 20.f);
            }
        }
    }
    */
    m_scenes.push_back(helmetScene);
}

void PbrApp::initShaders()
{
    m_pbrShader = Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs" , "../../shader/shader_pbr.fs");
    m_envmapShader = Cyan::createShader("EnvMapShader", "../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
    m_skyShader = Cyan::createShader("SkyShader", "../../shader/shader_sky.vs", "../../shader/shader_sky.fs");
}

void PbrApp::initEnvMaps()
{
    Cyan::Toolkit::ScopedTimer timer("initEnvMaps()", true);
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
    Cyan::Toolkit::ScopedTimer timer("init()", true);
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
    initHelmetScene();
    m_currentScene = 0u;

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
    m_indirectDiffuseSlider = 0.f;
    m_indirectSpecularSlider = 0.f;
    m_directLightingSlider = 1.f;
    m_indirectLightingSlider = 0.f;
    m_wrap = 0.1f;
    m_debugRay.init();
    m_debugRay.setColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    auto renderer = Cyan::Renderer::getSingletonPtr();
    m_debugRay.setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
    m_debugProbeIndex = 13;

    // clear color
    Cyan::getCurrentGfxCtx()->setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.f));

    // font
    ImGuiIO& io = ImGui::GetIO();
    m_font = io.Fonts->AddFontFromFileTTF("C:\\summerwars\\cyanRenderEngine\\lib\\imgui\\misc\\fonts\\Roboto-Medium.ttf", 16.f);
    timer.end();
}

void PbrApp::beginFrame()
{
    Cyan::getCurrentGfxCtx()->clear();
    Cyan::getCurrentGfxCtx()->setViewport({ 0, 0, static_cast<u32>(gEngine->getWindow().width), static_cast<u32>(gEngine->getWindow().height) });

    auto updateMatlInstanceData = [&](Cyan::MaterialInstance* matl) {
        matl->set("directDiffuseSlider", m_directDiffuseSlider);
        matl->set("directSpecularSlider", m_directSpecularSlider);
        matl->set("indirectDiffuseSlider", m_indirectDiffuseSlider);
        matl->set("indirectSpecularSlider", m_indirectSpecularSlider);
        matl->set("wrap", m_wrap);
    };

    updateMatlInstanceData(m_helmetMatl);
    updateMatlInstanceData(m_roomMatl);
    updateMatlInstanceData(m_sphereMatl);
    updateMatlInstanceData(m_cubeMatl);
    updateMatlInstanceData(m_coneMatl);
    updateMatlInstanceData(m_floorMatl);
    updateMatlInstanceData(m_cornellMatl);
    updateMatlInstanceData(m_rayTracingMatl);

    // update probe
    SceneManager::getSingletonPtr()->setLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);
}

void PbrApp::run()
{
    // precomute thingy here
    {
        auto ctx = Cyan::getCurrentGfxCtx();
        beginFrame();
        // m_lightFieldProbe->sampleScene();
        m_probeVolume->sampleScene();
        endFrame();
        ctx->setRenderTarget(nullptr, 0u);
        // m_lightFieldProbe->save();
    }
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
    SceneNode* target = nullptr;
    RayCastInfo closestHit = { nullptr, nullptr, FLT_MAX };
    // ray intersection test against all the entities in the scene to find the closest intersection
    for (auto entity : m_scenes[m_currentScene]->entities)
    {
        RayCastInfo hitInfo = entity->intersectRay(ro, rd, camera.view); 
        if (hitInfo.t > 0.f && hitInfo < closestHit)
        {
            closestHit = hitInfo;
            printf("Cast a ray from mouse that hits %s \n", hitInfo.m_entity->m_name);
        }
    }
    glm::vec3 worldHit = computeMouseHitWorldSpacePos(camera, rd, closestHit);
    printf("Mouse hit world at (%.2f, %.2f, %.2f) \n", worldHit.x, worldHit.y, worldHit.z);
    return closestHit;
}

void PbrApp::update()
{
    gEngine->processInput();

    // camera
    u32 camIdx = 0u;
    Camera& camera = m_scenes[0]->cameras[camIdx];
    // Camera& camera = m_scenes[m_currentScene]->getActiveCamera();
    CameraManager::updateCamera(camera);

    // helmet scene
    Scene* scene = m_scenes[0];
    float pointLightsRotSpeed = .5f; // in degree
    float rotationSpeed = glm::radians(.5f);
    glm::quat qRot = glm::quat(cos(rotationSpeed * .5f), sin(rotationSpeed * .5f) * glm::normalize(glm::vec3(0.f, 1.f, 1.f)));
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
    // transform
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

    // configure window pos and size
    ImVec2 debugWindowSize(gEngine->getWindow().width - renderer->m_viewport.m_width, 
                           gEngine->getWindow().height);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(debugWindowSize);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    m_ui.beginWindow("Debug Utils", windowFlags);
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
                {
                    drawLightingWidgets();
                    ImGui::Separator();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("LightFieldProbe Ray Tracing Debug Tools"))
            {
                float v[3] = {
                    m_debugRayTracingNormal.x,
                    m_debugRayTracingNormal.y,
                    m_debugRayTracingNormal.z
                };
                ImGui::SliderFloat3("Reflection normal", v, -1.f, 1.f, "%.2f");
                m_debugRayTracingNormal.x = v[0];
                m_debugRayTracingNormal.y = v[1];
                m_debugRayTracingNormal.z = v[2];

                ImGui::InputInt("Trace probe index", &m_debugProbeIndex);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings"))
            {
                ImGui::Text("Settings tab");
                {
                    drawRenderSettings();
                    ImGui::Separator();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tools"))
            {
                ImGui::Text("This is the tools tab!");
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
        ImGui::Text("Number of draw calls:         %d", 100u);
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
    if (ImGui::Button("Create Point Light"))
    {
        sceneManager->createPointLight(m_scenes[m_currentScene], glm::vec3(0.9f), glm::vec3(0.f), 1.f);
    }
    // experimental
    if (ImGui::TreeNodeEx("Experienmental", baseFlags))
    {
        ImGui::Text("Roughness");
        ImGui::SameLine();
        // ImGui::SliderFloat("##Roughness", &m_roughness, 0.f, 1.f, "%.2f");
        // m_eratoMatl->set("uniformRoughness", m_roughness);
        
        ImGui::Text("Wrap");
        ImGui::SameLine();
        ImGui::SliderFloat("##Wrap", &m_wrap, 0.f, 1.f, "%.2f");
        ImGui::TreePop();
    }
    // directional Lights
    if (ImGui::TreeNodeEx("Directional Lights", baseFlags))
    {
        for (u32 i = 0; i < m_scenes[m_currentScene]->dLights.size(); ++i)
        {
            char nameBuf[50];
            sprintf_s(nameBuf, "DirLight %d", i);
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
}

void PbrApp::drawSceneViewport()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    auto renderer = m_graphicsSystem->getRenderer();
    Cyan::Viewport viewport = renderer->getViewport();
    ImGui::SetNextWindowSize(ImVec2(viewport.m_width, viewport.m_height));
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
        Cyan::Texture* m_renderOutput = renderer->m_outputColorTexture;
        ImGui::GetForegroundDrawList()->AddImage(reinterpret_cast<void*>((intptr_t)m_renderOutput->m_id), 
            a, b, ImVec2(0, 1), ImVec2(1, 0));

        // TODO: refactor this
        // ray picking
        if (bRayCast && !ImGuizmo::IsOver())
        {
            RayCastInfo hitInfo = castMouseRay(glm::vec2(a.x, a.y), glm::vec2(windowSize.x, windowSize.y - min.y));
            m_selectedEntity = hitInfo.m_entity;
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
    {
        std::vector<const char*> envMaps;
        u32 numProbes = Cyan::getNumProbes();
        for (u32 index = 0u; index < numProbes; ++index)
        {
            envMaps.push_back(Cyan::getProbe(index)->m_baseCubeMap->m_name.c_str());
        }
        m_ui.comboBox(envMaps.data(), numProbes, "EnvMap", &m_currentProbeIndex);

        std::vector<const char*> scenes;
        for (u32 index = 0u; index < (u32)m_scenes.size(); ++index)
        {
            scenes.push_back(m_scenes[index]->m_name.c_str());
        }
        m_ui.comboBox(scenes.data(), scenes.size(), "Scene", &m_currentScene);
        ImGui::SameLine();
        if (m_ui.button("Load"))
        {

        }
        // ibl controls
        if (m_ui.header("IBL settings"))
        {
            if (m_ui.header("Direct lighting"))
            {
                ImGui::Text("Diffuse");
                ImGui::SameLine();
                ImGui::SliderFloat("##Diffuse", &m_directDiffuseSlider, 0.f, 1.f, "%.2f");
                ImGui::Text("Specular");
                ImGui::SameLine();
                ImGui::SliderFloat("##Specular", &m_directSpecularSlider, 0.f, 1.f, "%.2f");
            }
            if (m_ui.header("Indirect lighting"))
            {
                ImGui::Text("Diffuse");
                ImGui::SameLine();
                ImGui::SliderFloat("##IndirectDiffuse", &m_indirectDiffuseSlider, 0.f, 10.f, "%.2f");
                ImGui::Text("Specular");
                ImGui::SameLine();
                ImGui::SliderFloat("##IndirectSpecular", &m_indirectSpecularSlider, 0.f, 10.f, "%.2f");
            }
        }
        if (m_ui.header("Post-Processing"))
        {
            // bloom settings
            ImGui::Text("Bloom");
            ImGui::SameLine();
            ImGui::Checkbox("##Enabled", &renderer->m_bloom); 

            // exposure settings
            ImGui::Text("Exposure");
            ImGui::SameLine();
            ImGui::SliderFloat("##Exposure", &renderer->m_exposure, 0.f, 10.f, "%.2f");
        }
        if (m_ui.header("Debug view"))
        {
            const char* options[] = { "None", "D", "Fresnel", "G"};
            m_ui.comboBox(options, 4u, "PBR components", &m_debugViewIndex);
            switch (m_debugViewIndex)
            {
                case 0:
                    m_helmetMatl->set("debugD", 0.f);
                    m_helmetMatl->set("debugF", 0.f);
                    m_helmetMatl->set("debugG", 0.f);
                    break;
                case 1:
                    m_helmetMatl->set("debugD", 1.f);
                    m_helmetMatl->set("debugF", 0.f);
                    m_helmetMatl->set("debugG", 0.f);
                    break;
                case 2:
                    m_helmetMatl->set("debugD", 0.f);
                    m_helmetMatl->set("debugF", 1.f);
                    m_helmetMatl->set("debugG", 0.f);
                    break;
                case 3:
                    m_helmetMatl->set("debugD", 0.f);
                    m_helmetMatl->set("debugF", 0.f);
                    m_helmetMatl->set("debugG", 1.f);
                    break;
                default:
                    break;
            }
            ImGui::Checkbox("Use Disney reparameterization", &bDisneyReparam);
            float disneyReparam = 1.f ? bDisneyReparam : 0.f;
            m_helmetMatl->set("disneyReparam", disneyReparam);
        }
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
        auto renderer = Cyan::Renderer::getSingletonPtr();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        {
            f32* data = reinterpret_cast<f32*>(glMapNamedBuffer(m_app->m_debugRayBoundryBuffer->m_ssbo, GL_READ_WRITE));
            char* dataCpy = (char*)data;
            f32 numBoundryPoints = *data;
            data += 4;
            for (u32 i = 0; i < numBoundryPoints; ++i)
            {
                glm::vec4 vert0;
                glm::vec4 vert1;
                memcpy(&vert0.x, data, sizeof(glm::vec4));
                memcpy(&vert1.x, data + 4, sizeof(glm::vec4));
                glm::vec3 v0Vec3 = glm::vec3(vert0.x, vert0.y, vert0.z);
                glm::vec3 v1Vec3 = glm::vec3(vert1.x, vert1.y, vert1.z);
                m_debugBoundryRay[i]->setVerts(v0Vec3, v1Vec3);
                glm::mat4 model(1.f);
                m_debugBoundryRay[i]->setModel(model);
                m_debugBoundryRay[i]->setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
                m_debugBoundryRay[i]->draw();
                data += 8;
            }
            memset(dataCpy, 0x0, m_app->m_debugRayBoundryBuffer->m_totalSize);
            glUnmapNamedBuffer(m_app->m_debugRayBoundryBuffer->m_ssbo);
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
            // f32* data = reinterpret_cast<f32*>(glMapNamedBuffer(m_debugProbeLineBuffer->m_ssbo, GL_READ_WRITE));
            // char* dataCopy = (char*)data;
            // f32 numSegments = *data;
            // // skip padding
            // data += 4;
            // for (u32 i = 0; i < numSegments; ++i)
            // {
            //     glm::vec4 vert0;
            //     glm::vec4 vert1;
            //     memcpy(&vert0.x, data, sizeof(glm::vec4));
            //     memcpy(&vert1.x, data + 4, sizeof(glm::vec4));
            //     glm::vec3 v0Vec3 = glm::vec3(vert0.x, vert0.y, vert0.z);
            //     glm::vec3 v1Vec3 = glm::vec3(vert1.x, vert1.y, vert1.z);
            //     m_debugProbeLine->setVerts(v0Vec3, v1Vec3);
            //     glm::mat4 model(1.f);
            //     m_debugProbeLine->setModel(model);
            //     m_debugProbeLine->setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
            //     m_debugProbeLine->draw();
            //     data += 8;
            // }
            // memset(dataCopy, 0x0, m_debugProbeLineBuffer->m_totalSize);
            // glUnmapNamedBuffer(m_debugProbeLineBuffer->m_ssbo);
        }

        // draw projected debug ray on top of the octmap
        {
            // data = reinterpret_cast<f32*>(glMapNamedBuffer(m_app->m_debugRayOctBuffer->m_ssbo, GL_READ_WRITE));
            // dataCopy = (char*)data;
            // glm::vec2 vp0;
            // glm::vec2 vp1;
            // // i32 numBoundryPoints = *reinterpret_cast<i32*>(data);
            // // +2 to skipp paddings bytes
            // data += 2;
            // auto ctx = Cyan::getCurrentGfxCtx();
            // ctx->setRenderTarget(m_renderTarget, 0u);
            // ctx->setViewport(m_viewport);
            // // TODO: why the line data is not updated in real-time?
            // for (i32 i = 0; i < i32(numBoundryPoints); ++i)
            // {
            //     memcpy(&vp0.x, data, sizeof(glm::vec2));
            //     memcpy(&vp1.x, data + 2, sizeof(glm::vec2));
            //     data += 4;
            //     m_debugRaySegments[i]->setVerts(glm::vec3(vp0, 0.f), glm::vec3(vp1, 0.f));
            //     m_debugRaySegments[i]->setColor(glm::vec4(0.2f, 0.8f, .2f, 1.f));
            //     m_debugRaySegments[i]->draw();
            // }
            // memset(dataCopy, 0x0, m_app->m_debugRayOctBuffer->m_totalSize);
            // glUnmapNamedBuffer(m_app->m_debugRayOctBuffer->m_ssbo);
        }
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

struct UniformData
{
    u32 m_handle;
    void* m_data;
};

// TODO: how to not have to manually do this
template<typename T> 
struct SharedMaterialData
{
    T m_data;
    const char* m_uniformName;
    std::vector<Cyan::MaterialInstance*> m_listeners;

    void onUpdate() 
    { 
        for (auto matl : m_listeners)
        {
            matl->set(m_uniformName, m_data);
        }
    }

    void setData(T data)
    {
        m_data = data;
        onUpdate()
    }
};

void PbrApp::render()
{
    // frame timer
    Cyan::Toolkit::ScopedTimer frameTimer("render()");
    auto renderer = m_graphicsSystem->getRenderer();

    // ui
    m_ui.begin();
    drawDebugWindows();

    // update probe
    SceneManager::getSingletonPtr()->setLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);
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

    renderer->beginRender();
    // rendering
    // TODO: this should be bundled together with scene pass
    renderer->addDirectionalShadowPass(m_scenes[m_currentScene], 0u);
    renderer->addScenePass(m_scenes[m_currentScene]);
    {
        void* preallocated = renderer->getAllocator().alloc(sizeof(RayTracingDebugPass));
        auto renderTarget = m_lightFieldProbe->m_octMapRenderTarget;
        RayTracingDebugPass* pass = new (preallocated) RayTracingDebugPass(renderTarget, Cyan::Viewport{0u,0u, renderTarget->m_width, renderTarget->m_height}, this);
        renderer->addCustomPass(pass);
    }
#if DRAW_DEBUG
    {
        void* preallocated = renderer->getAllocator().alloc(sizeof(DebugAABBPass));
        Cyan::RenderTarget* sceneRenderTarget = renderer->getSceneColorRenderTarget();
        DebugAABBPass* pass = new (preallocated) DebugAABBPass(sceneRenderTarget, Cyan::Viewport{0u,0u, sceneRenderTarget->m_width, sceneRenderTarget->m_height}, m_scenes[m_currentScene]);
        renderer->addCustomPass(pass);
    }
#endif
    // TODO: how to exclude things in the scene from being post processed
    renderer->addPostProcessPasses();
    auto rt = renderer->getRenderOutputRenderTarget();
    // debug visualization
    {
        renderer->addTexturedQuadPass(rt, {rt->m_width - 320, rt->m_height - 180, 320, 180}, renderer->m_sceneNormalTextureSSAA);
        renderer->addTexturedQuadPass(rt, {rt->m_width - 320, rt->m_height - 360, 320, 180}, renderer->m_sceneDepthTextureSSAA);
        // renderer->addTexturedQuadPass(rt, {rt->m_width - 320, rt->m_height - 540, 320, 180}, renderer->m_ssaoTexture);
        renderer->addTexturedQuadPass(rt, {rt->m_width - 360, rt->m_height - 540, 360, 360}, m_probeVolume->m_probes[0]->m_radianceOct);
        renderer->addTexturedQuadPass(rt, {rt->m_width - 360, rt->m_height - 720, 180, 180}, m_probeVolume->m_probes[0]->m_normalOct);
        renderer->addTexturedQuadPass(rt, {rt->m_width - 180, rt->m_height - 720, 180, 180}, m_probeVolume->m_probes[0]->m_distanceOct);
    }
    renderer->render();
    renderer->endRender();

    drawSceneViewport();

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