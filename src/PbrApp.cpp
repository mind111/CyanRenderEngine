#include <iostream>
#include <queue>
#include <functional>

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

namespace Pbr
{
    void mouseCursorCallback(double cursorX, double cursorY, double deltaX, double deltaY)
    {
        PbrApp* app = PbrApp::get();
        app->bOrbit ? app->orbitCamera(deltaX, deltaY) : app->rotateCamera(deltaX, deltaY);
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
                    app->bRayCast = true;
                }
                else if (action == CYAN_RELEASE)
                {
                    app->bRayCast = false;
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
        const f32 speed = 0.3f;
        Scene* currentScene = app->m_scenes[app->m_currentScene];
        // translate the camera along its lookAt direciton
        glm::vec3 translation = currentScene->mainCamera.forward * (float)(speed * yOffset);
        currentScene->mainCamera.position += translation;
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
{
    bOrbit = false;
    gApp = this;
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
    // setup scenes
    Scene* helmetScene = Cyan::createScene("helmet_scene", "../../scene/default_scene/scene_config.json");
    glm::vec2 viewportSize = gEngine->getRenderer()->getViewportSize();
    float aspectRatio = viewportSize.x / viewportSize.y;
    helmetScene->mainCamera.projection = glm::perspective(glm::radians(helmetScene->mainCamera.fov), aspectRatio, helmetScene->mainCamera.n, helmetScene->mainCamera.f);

    Entity* envMapEntity = SceneManager::createEntity(helmetScene, "Envmap", Transform());
    envMapEntity->m_sceneRoot->attach(Cyan::createSceneNode("CubeMesh", Transform(), Cyan::getMesh("CubeMesh")));
    envMapEntity->setMaterial("CubeMesh", 0, m_skyMatl);

    // additional spheres for testing lighting
    PbrMaterialInputs inputs = { 0 };
    Entity* sphereEntity = SceneManager::getEntity(helmetScene, "Sphere0");
    Cyan::Texture* sphereAlbedo = Cyan::Toolkit::createFlatColorTexture("sphere_albedo", 1024u, 1024u, glm::vec4(0.8f, 0.8f, 0.6f, 1.f));
    inputs.m_baseColor = sphereAlbedo;
    inputs.m_uRoughness = 0.8f;
    inputs.m_uMetallic = 0.1f;
    m_sphereMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
    sphereEntity->setMaterial("SphereMesh", 0, m_sphereMatl);

    // open room
    Entity* floor = SceneManager::getEntity(helmetScene, "Floor");
    Cyan::Texture* floorAlbedo = Cyan::Toolkit::createFlatColorTexture("FloorAlbedo", 1024u, 1024u, glm::vec4(1.00f, 0.90, 0.80, 1.f));
    inputs.m_baseColor = floorAlbedo;
    inputs.m_uRoughness = 0.8f;
    inputs.m_uMetallic = 0.1f;
    m_floorMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
    floor->setMaterial("CubeMesh", 0, m_floorMatl);
    Entity* frontWall = SceneManager::getEntity(helmetScene, "Wall_0");
    frontWall->setMaterial("CubeMesh", 0, m_floorMatl);
    Entity* sideWall = SceneManager::getEntity(helmetScene, "Wall_1");
    sideWall->setMaterial("CubeMesh", 0, m_floorMatl);

    m_helmetMatl = Cyan::createMaterial(m_pbrShader)->createInstance();
    // TODO: create a .cyanmatl file for defining materials?
    m_helmetMatl->bindTexture("diffuseMaps[0]", Cyan::getTexture("helmet_diffuse"));
    m_helmetMatl->bindTexture("normalMap", Cyan::getTexture("helmet_nm"));
    m_helmetMatl->bindTexture("metallicRoughnessMap", Cyan::getTexture("helmet_roughness"));
    m_helmetMatl->bindTexture("aoMap", Cyan::getTexture("helmet_ao"));
    m_helmetMatl->bindTexture("envmap", m_envmap);
    m_helmetMatl->bindBuffer("dirLightsData", helmetScene->m_dirLightsBuffer);
    m_helmetMatl->bindBuffer("pointLightsData", helmetScene->m_pointLightsBuffer);
    m_helmetMatl->set("hasAoMap", 1.f);
    m_helmetMatl->set("hasNormalMap", 1.f);
    m_helmetMatl->set("kDiffuse", 1.0f);
    m_helmetMatl->set("kSpecular", 1.0f);
    m_helmetMatl->set("hasMetallicRoughnessMap", 1.f);
    m_helmetMatl->set("disneyReparam", 1.f);
    Entity* helmetEntity = SceneManager::getEntity(helmetScene, "DamagedHelmet");
    helmetEntity->getSceneNode("HelmetMesh")->m_meshInstance->setMaterial(0, m_helmetMatl);

    // cube
    // TODO: default material parameters
    Entity* cube = SceneManager::getEntity(helmetScene, "Cube");
    Cyan::Texture* cubeAlbedo = Cyan::Toolkit::createFlatColorTexture("CubeAlbedo", 128, 128, glm::vec4(0.6, 0.6, 0.6, 1.0));
    inputs.m_baseColor = cubeAlbedo;
    inputs.m_uRoughness = 0.8f;
    inputs.m_uMetallic = 0.0f;
    m_cubeMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
    cube->setMaterial("UvCubeMesh", 0, m_cubeMatl);

    Cyan::Texture* coneAlbedo = Cyan::Toolkit::createFlatColorTexture("ConeAlbedo", 64, 64, glm::vec4(1.0, 0.8, 0.8, 1.0));
    inputs.m_baseColor = coneAlbedo;
    inputs.m_uRoughness = 0.2f;
    inputs.m_uMetallic = 0.1f;
    Entity* cone = SceneManager::getEntity(helmetScene, "Cone");
    m_coneMatl = createDefaultPbrMatlInstance(helmetScene, inputs);
    cone->setMaterial("ConeMesh", 0, m_coneMatl);

    // add lights into the scene
    SceneManager::createPointLight(helmetScene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.0f, 0.0f, 1.5f), 4.f);
    SceneManager::createPointLight(helmetScene, glm::vec3(0.6, 0.65f, 2.86f), glm::vec3(0.0f, 0.8f, -2.4f), 4.f);
    // top light
    SceneManager::createDirectionalLight(helmetScene, glm::vec3(1.0f, 1.0, 1.0f), glm::normalize(glm::vec3(0.2f, 1.0f, 0.2f)), 1.f);

    // manage entities
    SceneNode* helmetNode = helmetEntity->getSceneNode("HelmetMesh");
    Transform centerTransform;
    centerTransform.m_translate = helmetNode->m_localTransform.m_translate;
    Entity* pivotEntity = SceneManager::createEntity(helmetScene, "PointLightCenter", centerTransform);
    Entity* pointLightEntity = helmetScene->m_rootEntity->detachChild("PointLight0");
    if (pointLightEntity) 
    {
        pivotEntity->attach(pointLightEntity);
    }
    m_scenes.push_back(helmetScene);
}

void PbrApp::initSpheresScene()
{
    const u32 kNumRows = 4u;
    const u32 kNumCols = 6u;

    Scene* sphereScene = Cyan::createScene("spheres_scene", "../../scene/default_scene/scene_spheres.json");
    sphereScene->mainCamera.projection = glm::perspective(glm::radians(sphereScene->mainCamera.fov), (float)(gEngine->getWindow().width) / gEngine->getWindow().height, sphereScene->mainCamera.n, sphereScene->mainCamera.f);

    Entity* envmapEntity = SceneManager::createEntity(sphereScene, "Envmap", Transform());
    envmapEntity->m_sceneRoot->attach(Cyan::createSceneNode("sphere_mesh", Transform(), Cyan::getMesh("sphere_mesh")));
    envmapEntity->getSceneNode("sphere_mesh")->m_meshInstance->setMaterial(0, m_envmapMatl);

    // add lights into the scene
    SceneManager::createDirectionalLight(sphereScene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(0.f, 0.f, -1.f), 2.f);
    SceneManager::createPointLight(sphereScene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.4f, 1.5f, 2.4f), 1.f);

    Cyan::Material* materialType = Cyan::createMaterial(m_pbrShader);
    Cyan::Texture* m_sphereAlbedo = Cyan::Toolkit::createFlatColorTexture("albedo", 1024u, 1024u, glm::vec4(0.4f, 0.4f, 0.4f, 1.f));
    for (u32 row = 0u; row < kNumRows; ++row)
    {
        for (u32 col = 0u; col < kNumCols; ++col)
        {
            u32 idx = row * kNumCols + col;
            // create entity
            glm::vec3 topLeft(-1.5f, 0.f, -1.8f);
            Transform transform = {
                topLeft + glm::vec3(0.5f * col, -0.5f * row, 0.f),
                glm::quat(1.f, glm::vec3(0.f)), // identity quaternion
                glm::vec3(.2f)
            };
            Entity* entity = SceneManager::createEntity(sphereScene, nullptr, transform);

            // create material
            m_sphereMatls[idx] = materialType->createInstance();
            m_sphereMatls[idx]->bindTexture("diffuseMaps[0]", m_sphereAlbedo);
            m_sphereMatls[idx]->bindTexture("envmap", m_envmap);
            m_sphereMatls[idx]->bindBuffer("dirLightsData", sphereScene->m_dirLightsBuffer);
            m_sphereMatls[idx]->bindBuffer("pointLightsData", sphereScene->m_pointLightsBuffer);

            m_sphereMatls[idx]->set("hasAoMap", 0.f);
            m_sphereMatls[idx]->set("hasNormalMap", 0.f);
            m_sphereMatls[idx]->set("hasRoughnessMap", 0.f);
            m_sphereMatls[idx]->set("kDiffuse", 1.0f);
            m_sphereMatls[idx]->set("kSpecular", 1.0f);
            // roughness increases in horizontal direction
            m_sphereMatls[idx]->set("uniformRoughness", 0.2f * col);
            // metallic increases in vertical direction
            m_sphereMatls[idx]->set("uniformMetallic", 1.0f);

            m_sphereMatls[idx]->set("directDiffuseSlider", 1.f);
            m_sphereMatls[idx]->set("directSpecularSlider", 1.f);
            m_sphereMatls[idx]->set("indirectDiffuseSlider", 1.f);
            m_sphereMatls[idx]->set("indirectSpecularSlider", 1.f);
            entity->getSceneNode("sphere_mesh")->m_meshInstance->setMaterial(0, m_sphereMatls[idx]);
        }
    }

    m_scenes.push_back(sphereScene);
}

void PbrApp::initShaders()
{
    m_pbrShader = Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs" , "../../shader/shader_pbr.fs");
    m_envmapShader = Cyan::createShader("EnvMapShader", "../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
    m_skyShader = Cyan::createShader("SkyShader", "../../shader/shader_sky.vs", "../../shader/shader_sky.fs");
}

void PbrApp::initEnvMaps()
{
    // image-based-lighting
    Cyan::Toolkit::createLightProbe("grace-new", "../../asset/cubemaps/grace-new.hdr", true);
    Cyan::Toolkit::createLightProbe("glacier", "../../asset/cubemaps/glacier.hdr",     true);
    Cyan::Toolkit::createLightProbe("ennis", "../../asset/cubemaps/ennis.hdr",         true);
    Cyan::Toolkit::createLightProbe("pisa", "../../asset/cubemaps/pisa.hdr",           true);
    Cyan::Toolkit::createLightProbe("doge2", "../../asset/cubemaps/doge2.hdr",         true);
    Cyan::Toolkit::createLightProbe("studio", "../../asset/cubemaps/studio_01_4k.hdr",  true);
    Cyan::Toolkit:: createLightProbe("fire-sky", "../../asset/cubemaps/the_sky_is_on_fire_4k.hdr",  true);
    m_currentProbeIndex = 3u;
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;

    m_envmapMatl = Cyan::createMaterial(m_envmapShader)->createInstance(); 
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);
    m_skyMatl = Cyan::createMaterial(m_skyShader)->createInstance();
    
    // this is necessary as we are setting z component of
    // the cubemap mesh to 1.f
    glDepthFunc(GL_LEQUAL);
}

void PbrApp::initUniforms()
{
    // create uniforms
    u_numPointLights = Cyan::createUniform("numPointLights", Uniform::Type::u_int);
    u_numDirLights = Cyan::createUniform("numDirLights", Uniform::Type::u_int);
    u_kDiffuse = Cyan::createUniform("kDiffuse", Uniform::Type::u_float);
    u_kSpecular = Cyan::createUniform("kSpecular", Uniform::Type::u_float);
    u_cameraProjection = Cyan::createUniform("s_projection", Uniform::Type::u_mat4);
    u_cameraView = Cyan::createUniform("s_view", Uniform::Type::u_mat4);
    u_hasNormalMap = Cyan::createUniform("hasNormalMap", Uniform::Type::u_float); 
    u_hasAoMap = Cyan::createUniform("hasAoMap", Uniform::Type::u_float); 
    Uniform* u_hasRoughnessMap = Cyan::createUniform("hasRoughnessMap", Uniform::Type::u_float);
    Uniform* u_uniformRoughness = Cyan::createUniform("uniformRoughness", Uniform::Type::u_float);
    Uniform* u_uniformMetallic = Cyan::createUniform("uniformMetallic", Uniform::Type::u_float);
}


void PbrApp::init(int appWindowWidth, int appWindowHeight, glm::vec2 sceneViewportPos, glm::vec2 renderSize)
{
    using Cyan::Material;
    using Cyan::Mesh;

    // init engine
    gEngine->init({ appWindowWidth, appWindowHeight }, sceneViewportPos, renderSize);
    bRunning = true;
    // setup input control
    gEngine->registerMouseCursorCallback(&Pbr::mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&Pbr::mouseButtonCallback);
    gEngine->registerMouseScrollWheelCallback(&Pbr::mouseScrollWheelCallback);

    initShaders();
    initUniforms();
    initEnvMaps();
    initHelmetScene();
    // initSpheresScene();
    m_currentScene = 0u;

    // ui
    m_ui.init(gEngine->getWindow().mpWindow);

    // misc
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
    m_indirectSpecularSlider = 1.f;
    m_directLightingSlider = 1.f;
    m_indirectLightingSlider = 1.f;
    m_wrap = 0.1f;
    m_debugRay.init();
    m_debugRay.setColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    m_debugRay.setViewProjection(gEngine->getRenderer()->u_cameraView, gEngine->getRenderer()->u_cameraProjection);

    // visualizer
    m_bufferVis = {};
    m_bufferVis.init(m_helmetMatl->m_uniformBuffer, glm::vec2(-0.6f, 0.8f), 0.8f, 0.05f);
    // clear color
    Cyan::getCurrentGfxCtx()->setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.f));

    // font
    ImGuiIO& io = ImGui::GetIO();
    m_font = io.Fonts->AddFontFromFileTTF("C:\\summerwars\\cyanRenderEngine\\lib\\imgui\\misc\\fonts\\Roboto-Medium.ttf", 16.f);
}

void PbrApp::beginFrame()
{
    Cyan::getCurrentGfxCtx()->clear();
    Cyan::getCurrentGfxCtx()->setViewport({ 0, 0, static_cast<u32>(gEngine->getWindow().width), static_cast<u32>(gEngine->getWindow().height) });
    m_ui.begin();
}

void PbrApp::run()
{
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

RayCastInfo PbrApp::castMouseRay(const glm::vec2& currentViewportPos, const glm::vec2& currentViewportSize)
{
    // convert mouse cursor pos to view space 
    Cyan::Viewport viewport = gEngine->getRenderer()->getViewport();
    glm::vec2 viewportPos = gEngine->getSceneViewportPos();
    double mouseCursorX = m_mouseCursorX - currentViewportPos.x;
    double mouseCursorY = m_mouseCursorY - currentViewportPos.y;
    // NDC space
    glm::vec2 uv(2.0 * mouseCursorX / currentViewportSize.x - 1.0f, 2.0 * (currentViewportSize.y - mouseCursorY) / currentViewportSize.y - 1.0f);
    // homogeneous clip space
    glm::vec4 q = glm::vec4(uv, -0.8, 1.0);
    glm::mat4 projInverse = glm::inverse(m_scenes[m_currentScene]->mainCamera.projection);
    // q.z must be less than zero after this evaluation
    q = projInverse * q;
    // view space
    q /= q.w;
    glm::vec3 rd = glm::normalize(glm::vec3(q.x, q.y, q.z));
    // in view space
    m_debugRay.setVerts(glm::vec3(0.f, 0.f, -0.2f), glm::vec3(rd * 20.0f));
    glm::mat4 viewInverse = glm::inverse(m_scenes[m_currentScene]->mainCamera.view);
    m_debugRay.setModel(viewInverse);

    glm::vec3 ro = glm::vec3(0.f);
    SceneNode* target = nullptr;
    RayCastInfo closestHit = { nullptr, nullptr, FLT_MAX };
    // ray intersection test against all the entities in the scene to find the closest intersection
    for (auto entity : m_scenes[m_currentScene]->entities)
    {
        RayCastInfo hitInfo = entity->intersectRay(ro, rd, m_scenes[m_currentScene]->mainCamera.view); 
        if (hitInfo.t > 0.f && hitInfo < closestHit)
        {
            closestHit = hitInfo;
            printf("Cast a ray from mouse that hits %s \n", hitInfo.m_entity->m_name);
        }
    }
    return closestHit;
}

void PbrApp::update()
{
    gEngine->processInput();

    // camera
    Camera& camera = m_scenes[m_currentScene]->mainCamera;
    CameraManager::updateCamera(camera);

    // helmet scene
    Scene* scene = m_scenes[0];
    float pointLightsRotSpeed = .5f; // in degree
    float rotationSpeed = glm::radians(.5f);
    glm::quat qRot = glm::quat(cos(rotationSpeed * .5f), sin(rotationSpeed * .5f) * glm::normalize(glm::vec3(0.f, 1.f, 1.f)));
    Entity* pivotEntity = SceneManager::getEntity(m_scenes[0], "PointLightCenter");

    pivotEntity->applyLocalRotation(qRot);
    scene->pLights[0].position = glm::vec4(scene->pLights[0].baseLight.m_entity->m_sceneRoot->m_worldTransform.m_translate, 1.0f); 
}

void PbrApp::drawDebugWindows()
{
    // configure window pos and size
    ImVec2 debugWindowSize(gEngine->getWindow().width - gEngine->getRenderer()->m_viewport.m_width, 
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
                    drawSceneGraphUI(m_scenes[m_currentScene]->m_rootEntity);
                    ImGui::Separator();
                }
                {
                    drawLightingWidgets();
                    ImGui::Separator();
                }
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
        ImGui::Checkbox("Super Sampling 4x", &gEngine->getRenderer()->m_bSuperSampleAA);
    }
}

void PbrApp::drawSceneGraphUI(Entity* entity) 
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
                drawSceneGraphUI(child);
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
    // experimental
    if (ImGui::TreeNodeEx("Experienmental", baseFlags))
    {
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
                ImGui::InputFloat("##Intensity", &m_scenes[m_currentScene]->dLights[i].baseLight.color.w);
                ImGui::Text("Color:");
                ImGui::ColorPicker3("##Color", &m_scenes[m_currentScene]->dLights[i].baseLight.color.r);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
    // point lights
    ImGui::TreeNodeEx("Point Lights", baseFlags);
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
                ImGui::Text("Color:");
                ImGui::ColorPicker3("##Color", &m_scenes[m_currentScene]->pLights[i].baseLight.color.r);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

void PbrApp::drawSceneViewport()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    Cyan::Viewport viewport = gEngine->getRenderer()->getViewport();
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
        Cyan::Texture* m_renderOutput = gEngine->getRenderer()->m_outputColorTexture;
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
            if (ImGuizmo::Manipulate(&m_scenes[m_currentScene]->mainCamera.view[0][0], 
                                &m_scenes[m_currentScene]->mainCamera.projection[0][0],
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
            ImGui::Checkbox("##Enabled", &gEngine->getRenderer()->m_bloom); 

            // exposure settings
            ImGui::Text("Exposure");
            ImGui::SameLine();
            ImGui::SliderFloat("##Exposure", &gEngine->getRenderer()->m_exposure, 0.f, 10.f, "%.2f");
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
                    BoundingBox3f aabb = node->m_meshInstance->getAABB();
                    if (aabb.isValid)
                    {
                        aabb.setViewProjection(renderer->u_cameraView, renderer->u_cameraProjection);
                        aabb.draw(node->m_worldTransform.toMatrix());
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

void PbrApp::render()
{
    // frame timer
    Cyan::Toolkit::ScopedTimer frameTimer("render()");
    Cyan::Renderer* renderer = gEngine->getRenderer();


    // TODO: how to not have to manually do this
    struct SharedMaterialData
    {
        void* m_data;
        std::vector<Cyan::MaterialInstance*> m_listeners;

        void onUpdate() { }
        void notify() { }
    };

    auto updateMatlInstanceData = [&](Cyan::MaterialInstance* matl) {
        matl->set("directDiffuseSlider", m_directDiffuseSlider);
        matl->set("directSpecularSlider", m_directSpecularSlider);
        matl->set("indirectDiffuseSlider", m_indirectDiffuseSlider);
        matl->set("indirectSpecularSlider", m_indirectSpecularSlider);
        matl->set("wrap", m_wrap);
    };

    updateMatlInstanceData(m_helmetMatl);
    updateMatlInstanceData(m_floorMatl);
    updateMatlInstanceData(m_sphereMatl);
    updateMatlInstanceData(m_cubeMatl);
    updateMatlInstanceData(m_coneMatl);

    // update probe
    SceneManager::setLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);

    // rendering
    renderer->beginRender();
    renderer->addScenePass(m_scenes[m_currentScene]);
    void* preallocated = renderer->getAllocator().alloc(sizeof(DebugAABBPass));
    Cyan::RenderTarget* sceneRenderTarget = renderer->getSceneColorRenderTarget();
    DebugAABBPass* pass = new (preallocated) DebugAABBPass(sceneRenderTarget, Cyan::Viewport{0u,0u, sceneRenderTarget->m_width, sceneRenderTarget->m_height}, m_scenes[m_currentScene]);
    renderer->addCustomPass(pass);
    // TODO: how to exclude things in the scene from being post processed
    renderer->addPostProcessPasses();
    {
        // test voxelization
        Entity* helmetEntity = SceneManager::getEntity(m_scenes[m_currentScene], "DamagedHelmet");
        SceneNode* sceneNode = helmetEntity->getSceneNode("HelmetMesh");
        glm::mat4 modelMatrix = sceneNode->getWorldTransform().toMatrix();
        m_voxelOutput = renderer->voxelizeMesh(sceneNode->m_meshInstance, &modelMatrix);
    }
    Cyan::RenderTarget* rt = renderer->getRenderOutputRenderTarget();
    renderer->addTexturedQuadPass(rt, {rt->m_width - 320, rt->m_height - 160, 320, 160}, m_voxelOutput);
    renderer->render();
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setRenderTarget(rt, 0u);
    ctx->setViewport({0, 0, rt->m_width, rt->m_height});
    ctx->setDepthControl(Cyan::DepthControl::kDisable);
    // visualizer
    m_bufferVis.draw();
    ctx->setDepthControl(Cyan::DepthControl::kEnable);
    renderer->endRender();

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

void PbrApp::orbitCamera(double deltaX, double deltaY)
{
    Camera& camera = m_scenes[m_currentScene]->mainCamera;
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