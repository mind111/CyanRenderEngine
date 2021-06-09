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
    // setup scenes
    Scene* scene = Cyan::createScene("../../scene/default_scene/scene_config.json");
    scene->mainCamera.projection = glm::perspective(glm::radians(scene->mainCamera.fov), (float)(gEngine->getWindow().width) / gEngine->getWindow().height, scene->mainCamera.n, scene->mainCamera.f);
    m_scenes.push_back(scene);
    currentScene = 0;

    // helmet instance 
    m_pbrShader = Cyan::createShader("PbrShader", "../../shader/shader_pbr.vs" , "../../shader/shader_pbr.fs");

    // create uniforms
    u_numPointLights = Cyan::createUniform("numPointLights", Uniform::Type::u_int);
    u_numDirLights = Cyan::createUniform("numDirLights", Uniform::Type::u_int);
    u_kDiffuse = Cyan::createUniform("kDiffuse", Uniform::Type::u_float);
    u_kSpecular = Cyan::createUniform("kSpecular", Uniform::Type::u_float);
    u_cameraProjection = Cyan::createUniform("s_projection", Uniform::Type::u_mat4);
    u_cameraView = Cyan::createUniform("s_view", Uniform::Type::u_mat4);
    u_hasNormalMap = Cyan::createUniform("hasNormalMap", Uniform::Type::u_float); 
    u_hasAoMap = Cyan::createUniform("hasAoMap", Uniform::Type::u_float); 
    m_dirLightsBuffer = Cyan::createRegularBuffer("dirLightsData", m_pbrShader, 1, sizeof(DirectionalLight) * 10);
    m_pointLightsBuffer = Cyan::createRegularBuffer("pointLightsData", m_pbrShader, 2, sizeof(PointLight) * 10);

    // image-based-lighting
    m_iblAssets = { };

    // envmap related
    m_envmapShader = Cyan::createShader("EnvMapShader", "../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
    m_envmapMatl = Cyan::createMaterial(m_envmapShader)->createInstance(); 
    m_envmap = Cyan::Toolkit::loadEquirectangularMap("grace-new", "../../asset/cubemaps/grace-new.hdr", true);
    glDepthFunc(GL_LEQUAL);
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);
    // Generate cubemap from a hdri equirectangular map
    Cyan::getMesh("cubemapMesh")->setMaterial(0, m_envmapMatl);
    // Generate diffuse irradiace map from envmap
    m_iblAssets.m_diffuse = Cyan::Toolkit::prefilterEnvMapDiffuse("diffuse_irradiance_map", m_envmap, true);
    m_iblAssets.m_specular = Cyan::Toolkit::prefilterEnvmapSpecular(m_envmap);
    m_iblAssets.m_brdfIntegral = Cyan::Toolkit::generateBrdfLUT();

    // init materials
    m_helmetMatl = Cyan::createMaterial(m_pbrShader)->createInstance();
    m_helmetMatl->bindTexture("diffuseMaps[0]", Cyan::getTexture("helmet_diffuse"));
    m_helmetMatl->bindTexture("normalMap", Cyan::getTexture("helmet_nm"));
    m_helmetMatl->bindTexture("roughnessMap", Cyan::getTexture("helmet_roughness"));
    m_helmetMatl->bindTexture("aoMap", Cyan::getTexture("helmet_ao"));
    m_helmetMatl->bindTexture("envmap", m_envmap);
    m_helmetMatl->bindTexture("irradianceDiffuse", m_iblAssets.m_diffuse);
    m_helmetMatl->bindTexture("irradianceSpecular", m_iblAssets.m_specular);
    m_helmetMatl->bindTexture("brdfIntegral", m_iblAssets.m_brdfIntegral);
    m_helmetMatl->set("hasAoMap", 1.f);
    m_helmetMatl->set("hasNormalMap", 1.f);
    m_helmetMatl->set("kDiffuse", 1.0f);
    m_helmetMatl->set("kSpecular", 1.0f);
    Mesh* helmetMesh = Cyan::getMesh("helmet_mesh");
    helmetMesh->setMaterial(0, m_helmetMatl);

    // add lights into the scene
    SceneManager::createDirectionalLight(*scene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(0.f, 0.f, -1.f), 2.f);
    SceneManager::createDirectionalLight(*scene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(-0.5f, -0.3f, -1.f), 2.f);
    SceneManager::createPointLight(*scene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.4f, 1.5f, 2.4f), 1.f);
    SceneManager::createPointLight(*scene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.0f, 0.8f, -2.4f), 1.f);

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

template <typename T>
u32 sizeofVector(const std::vector<T>& vec)
{
    CYAN_ASSERT(vec.size() > 0, "empty vector");
    return sizeof(vec[0]) * (u32)vec.size();
}

void PbrApp::update()
{
    gEngine->processInput();
}


void PbrApp::render()
{

    Camera& camera = m_scenes[currentScene]->mainCamera;
    CameraManager::updateCamera(camera);
    m_helmetMatl->set("numDirLights", (u32)m_scenes[currentScene]->dLights.size());
    m_helmetMatl->set("numPointLights", (u32)m_scenes[currentScene]->pLights.size());
    EXEC_ONCE(m_helmetMatl->m_uniformBuffer->debugPrint())

    Cyan::setUniform(u_cameraView, (void*)&camera.view[0]);
    Cyan::setUniform(u_cameraProjection, (void*)&camera.projection[0]);
    Cyan::setBuffer(m_pointLightsBuffer, m_scenes[currentScene]->pLights.data(), sizeofVector(m_scenes[currentScene]->pLights));
    Cyan::setBuffer(m_dirLightsBuffer, m_scenes[currentScene]->dLights.data(), sizeofVector(m_scenes[currentScene]->dLights));

    m_brdfDebugger.draw();

    Cyan::Renderer* renderer = gEngine->getRenderer();
    // draw entities in the scene
    renderer->render(m_scenes[0]);
    // envmap pass
    renderer->drawMesh(Cyan::getMesh("cubemapMesh"));
    // visualizer
    m_bufferVis.draw();
    // ui
    Entity* e = &m_scenes[currentScene]->entities[entityOnFocusIdx];
    ImGui::Begin("Transform");
    gEngine->displayFloat3("Translation", e->m_xform.translation);
    gEngine->displayFloat3("Scale", e->m_xform.scale, true);
    gEngine->displaySliderFloat("Scale", &e->m_xform.scale.x, 0.0f, 10.f);
    gEngine->displaySliderFloat("kd", (f32*)u_kDiffuse->m_valuePtr, 0.0f, 1.0f);
    gEngine->displaySliderFloat("ks", (f32*)u_kSpecular->m_valuePtr, 0.0f, 7.0f);
    e->m_xform.scale.z = e->m_xform.scale.y = e->m_xform.scale.x;
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    // flip
    renderer->endFrame();
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
    Camera& camera = m_scenes[currentScene]->mainCamera;
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