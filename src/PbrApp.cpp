#include <iostream>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "stb_image.h"

#include "CyanAPI.h"
#include "Light.h"
#include "PbrApp.h"
#include "Shader.h"
#include "Mesh.h"

#define DRAW_DEBUG_LINES 0

/* Constants */
// In radians per pixel 

static float kCameraOrbitSpeed = 0.005f;
static float kCameraRotateSpeed = 0.005f;

namespace Pbr
{

    void mouseCursorCallback(double deltaX, double deltaY)
    {
        PbrApp* app = PbrApp::get();
        app->bOrbit ? app->orbitCamera(deltaX, deltaY) : app->rotateCamera(deltaX, deltaY);
    }

    void mouseButtonCallback(int button, int action)
    {
        PbrApp* app = PbrApp::get();
        switch(button)
        {
            case CYAN_MOUSE_BUTTON_LEFT:
            {
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

void PbrApp::initHelmetScene()
{
    // setup scenes
    Scene* helmetScene = Cyan::createScene("../../scene/default_scene/scene_config.json");
    helmetScene->mainCamera.projection = glm::perspective(glm::radians(helmetScene->mainCamera.fov), (float)(gEngine->getWindow().width) / gEngine->getWindow().height, helmetScene->mainCamera.n, helmetScene->mainCamera.f);

    Transform transform = {
        glm::vec3(0.f),
        glm::quat(1.f, glm::vec3(0.f)), // identity quaternion
        glm::vec3(1.f)
    };
    Entity* envMapEntity = SceneManager::createEntity(helmetScene, "cubemapMesh");
    envMapEntity->m_meshInstance->setMaterial(0, m_envmapMatl);

    m_scenes.push_back(helmetScene);

    m_helmetMatl = Cyan::createMaterial(m_pbrShader)->createInstance();
    // TODO: create a .cyanmatl file for definine materials?
    m_helmetMatl->bindTexture("diffuseMaps[0]", Cyan::getTexture("helmet_diffuse"));
    m_helmetMatl->bindTexture("normalMap", Cyan::getTexture("helmet_nm"));
    m_helmetMatl->bindTexture("roughnessMap", Cyan::getTexture("helmet_roughness"));
    m_helmetMatl->bindTexture("aoMap", Cyan::getTexture("helmet_ao"));
    m_helmetMatl->bindTexture("envmap", m_envmap);
    m_helmetMatl->bindBuffer("dirLightsData", helmetScene->m_dirLightsBuffer);
    m_helmetMatl->bindBuffer("pointLightsData", helmetScene->m_pointLightsBuffer);
    m_helmetMatl->set("hasAoMap", 1.f);
    m_helmetMatl->set("hasNormalMap", 1.f);
    m_helmetMatl->set("kDiffuse", 1.0f);
    m_helmetMatl->set("kSpecular", 1.0f);
    m_helmetMatl->set("hasRoughnessMap", 1.f);
    m_helmetMatl->m_uniformBuffer->debugPrint();

    SceneManager::getEntity(m_scenes[0], 0)->m_meshInstance->setMaterial(0, m_helmetMatl);

    // add lights into the scene
    SceneManager::createDirectionalLight(*helmetScene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(0.f, 0.f, -1.f), 2.f);
    SceneManager::createDirectionalLight(*helmetScene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(-0.5f, -0.3f, -1.f), 2.f);
    SceneManager::createPointLight(*helmetScene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.4f, 1.5f, 2.4f), 1.f);
    SceneManager::createPointLight(*helmetScene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.0f, 0.8f, -2.4f), 1.f);
}

void PbrApp::initSpheresScene()
{
    const u32 kNumRows = 4u;
    const u32 kNumCols = 6u;

    Scene* sphereScene = Cyan::createScene("../../scene/default_scene/scene_spheres.json");
    sphereScene->mainCamera.projection = glm::perspective(glm::radians(sphereScene->mainCamera.fov), (float)(gEngine->getWindow().width) / gEngine->getWindow().height, sphereScene->mainCamera.n, sphereScene->mainCamera.f);

    Entity* envmapEntity = SceneManager::createEntity(sphereScene, "cubemapMesh");
    envmapEntity->m_meshInstance->setMaterial(0, m_envmapMatl);

    // add lights into the scene
    SceneManager::createDirectionalLight(*sphereScene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(0.f, 0.f, -1.f), 2.f);
    SceneManager::createPointLight(*sphereScene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.4f, 1.5f, 2.4f), 1.f);

    Cyan::Material* materialType = Cyan::createMaterial(m_pbrShader);
    Cyan::Texture* m_sphereAlbedo = Cyan::Toolkit::createFlatColorTexture("albedo", 1024u, 1024u, glm::vec4(0.4f, 0.4f, 0.4f, 1.f));
    for (u32 row = 0u; row < kNumRows; ++row)
    {
        for (u32 col = 0u; col < kNumCols; ++col)
        {
            u32 idx = row * kNumCols + col;
            // create entity
            // glm::vec3 topLeft(-1.5f, 1.5f, -1.8f);
            glm::vec3 topLeft(-1.5f, 0.f, -1.8f);
            Transform transform = {
                topLeft + glm::vec3(0.5f * col, -0.5f * row, 0.f),
                glm::quat(1.f, glm::vec3(0.f)), // identity quaternion
                glm::vec3(.2f)
            };
            Entity* entity = SceneManager::createEntity(sphereScene, "sphere_mesh", transform);

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

            entity->m_meshInstance->setMaterial(0, m_sphereMatls[idx]);
        }
    }

    m_scenes.push_back(sphereScene);
}

void PbrApp::initShaders()
{
    m_pbrShader = Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs" , "../../shader/shader_pbr.fs");
    m_envmapShader = Cyan::createShader("EnvMapShader", "../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
}

void PbrApp::initEnvMaps()
{
    // image-based-lighting
    Cyan::Toolkit::createLightProbe("grace-new", "../../asset/cubemaps/grace-new.hdr", true);
    Cyan::Toolkit::createLightProbe("glacier", "../../asset/cubemaps/glacier.hdr",     true);
    Cyan::Toolkit::createLightProbe("ennis", "../../asset/cubemaps/ennis.hdr",         true);
    Cyan::Toolkit::createLightProbe("pisa", "../../asset/cubemaps/pisa.hdr",           true);
    Cyan::Toolkit::createLightProbe("doge2", "../../asset/cubemaps/doge2.hdr",          true);
    m_currentProbeIndex = 0u;
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;

    m_envmapMatl = Cyan::createMaterial(m_envmapShader)->createInstance(); 
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);
    
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

void PbrApp::init(int appWindowWidth, int appWindowHeight)
{
    using Cyan::Material;
    using Cyan::Mesh;

    // init engine
    gEngine->init({ appWindowWidth, appWindowHeight});
    bRunning = true;
    // setup input control
    gEngine->registerMouseCursorCallback(&Pbr::mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&Pbr::mouseButtonCallback);

    initShaders();
    initUniforms();
    initEnvMaps();
    initHelmetScene();
    initSpheresScene();
    m_currentScene = 1u;

    // misc
    entityOnFocusIdx = 0;
    Cyan::setUniform(u_kDiffuse, 1.0f);
    Cyan::setUniform(u_kSpecular, 1.0f);
    Cyan::setUniform(u_hasNormalMap, 1.f);
    Cyan::setUniform(u_hasAoMap, 1.f);
    m_envMapDebugger = { };
    m_envMapDebugger.init(m_envmap);
    m_brdfDebugger = { };
    m_brdfDebugger.init();

    // visualizer
    m_bufferVis = {};
    m_bufferVis.init(m_helmetMatl->m_uniformBuffer, glm::vec2(-0.6f, 0.8f), 0.8f, 0.05f);
    // clear color
    Cyan::getCurrentGfxCtx()->setClearColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.f));

    // font
    ImGuiIO& io = ImGui::GetIO();
    m_font = io.Fonts->AddFontFromFileTTF("C:\\summerwars\\cyanRenderEngine\\lib\\imgui\\misc\\fonts\\Roboto-Medium.ttf", 16.f);
}

void PbrApp::beginFrame()
{
    Cyan::getCurrentGfxCtx()->clear();
    Cyan::getCurrentGfxCtx()->setViewport(0, 0, gEngine->getWindow().width, gEngine->getWindow().height);
    // ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
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

void PbrApp::update()
{
    gEngine->processInput();
}

void drawImGuiCombo(const char* items[], u32 numItems, const char* label, u32* currentIndex)
{
    if (ImGui::BeginCombo(label, items[*currentIndex], 0))
    {
        for (int n = 0; n < numItems; n++)
        {
            const bool isSelected = (*currentIndex == n);
            if (ImGui::Selectable(items[n], isSelected))
            {
                *currentIndex = n;
            }
            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

void PbrApp::render()
{
    static double lastFrameDurationInMs = 0.0;
    // frame timer
    Cyan::Toolkit::ScopedTimer frameTimer("render()");

    Cyan::Renderer* renderer = gEngine->getRenderer();

    // stats window
    ImGui::Begin("Frame stats");
    ImGui::PushFont(m_font);
    {
        ImGui::Text("Frame time:                   %.2f ms", lastFrameDurationInMs);
        ImGui::Text("Number of draw calls:         %d", 100u);
        ImGui::Text("Number of entities:           %d", m_scenes[m_currentScene]->entities.size());
    }
    ImGui::PopFont();
    ImGui::End();

    // scene window
    ImGui::Begin("Scene");
    ImGui::PushFont(m_font);
    {
        const char* envMaps[] = { "grace-new", "glacier", "ennis", "pisa", "doge2" };
        drawImGuiCombo(envMaps, 5u, "envMap", &m_currentProbeIndex);
        const char* scenes[] = { "helmet_scene", "spheres_scene" };
        drawImGuiCombo(scenes, 2u, "scene", &m_currentScene);
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {

        }
    }
    ImGui::PopFont();
    ImGui::End();

    // update probe
    SceneManager::setLightProbe(m_scenes[m_currentScene], Cyan::getProbe(m_currentProbeIndex));
    m_envmap = Cyan::getProbe(m_currentProbeIndex)->m_baseCubeMap;
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);

    // draw entities in the scene
    renderer->renderScene(m_scenes[m_currentScene]);

    // visualizer
    m_bufferVis.draw();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    // flip
    renderer->endFrame();
    frameTimer.end();
    lastFrameDurationInMs = frameTimer.m_durationInMs;
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