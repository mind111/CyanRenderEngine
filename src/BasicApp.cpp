#include <iostream>
#include <cmath>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "BasicApp.h"
#include "Shader.h"

/* Constants */
// In radians per pixel 
static float kCameraOrbitSpeed = 0.005f;
static float kCameraRotateSpeed = 0.005f;

void mouseCursorCallback(double deltaX, double deltaY)
{
    BasicApp* app = BasicApp::get();
    app->bOrbit ? app->orbitCamera(deltaX, deltaY) : app->rotateCamera(deltaX, deltaY);
}

void mouseButtonCallback(int button, int action)
{
    BasicApp* app = BasicApp::get();
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

static BasicApp* gApp = nullptr; 

BasicApp::BasicApp()
    : CyanApp()
{
    bOrbit = false;
    bRunning = false;
    gApp = this;
}

BasicApp* BasicApp::get()
{
    if (gApp)
        return gApp;
    return nullptr;
}

void BasicApp::init(int appWindowWidth, int appWindowHeight)
{
    /* Init engine instance */
    gEngine->setup({ appWindowWidth, appWindowHeight});
    bRunning = true;

    /* Setup inputs */
    gEngine->registerMouseCursorCallback(&mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&mouseButtonCallback);

    /* Setup ImGui */
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(gEngine->getWindow().mpWindow, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    /* ImGui style */
    ImGuiStyle& style = ImGui::GetStyle();
	style.ChildRounding = 3.f;
	style.GrabRounding = 0.f;
	style.WindowRounding = 0.f;
	style.ScrollbarRounding = 3.f;
	style.FrameRounding = 3.f;
	style.WindowTitleAlign = ImVec2(0.5f,0.5f);

	style.Colors[ImGuiCol_Text]                  = ImVec4(0.73f, 0.73f, 0.73f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.26f, 0.26f, 0.26f, 0.95f);
	style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
	style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	style.Colors[ImGuiCol_Border]                = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
	style.Colors[ImGuiCol_Button]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	style.Colors[ImGuiCol_Header]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
	style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.32f, 0.52f, 0.65f, 1.00f);
	style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);

    // Setup scenes
    Scene* scene = gEngine->loadScene("../../scene/default_scene/scene_config.json");
    m_scenes.push_back(scene);
    currentScene = 0;

    /* Displaying xform info */
    entityOnFocusIdx = 0;

    // Setup shaders
    // - register all the shaders that this demo will use
    // TODO: How to handle custom shader
    m_shaderVars = { };
    /* Init shader variables */
    // TODO: Following should really be part of material
    m_shaderVars.kAmbient = 0.2f;
    m_shaderVars.kDiffuse = 0.2f;
    m_shaderVars.kSpecular = 2.2f;
    m_shaderVars.projection = m_scenes[currentScene]->mainCamera.projection;

    m_shader = new BlinnPhongShader("../../shader/shader.vs", "../../shader/shader.fs");

    // TODO: Implement GfxContext class that handles rendering states ?
    // gEngine->GfxContext->bindShader();
    gEngine->getRenderer()->registerShader(m_shader);

    gEngine->getRenderer()->enableDepthTest();
    gEngine->getRenderer()->setClearColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.f));
}

void BasicApp::update()
{
    Camera& camera = m_scenes[currentScene]->mainCamera;
    CameraManager::updateCamera(camera);

    m_shaderVars.view = camera.view; 
    m_shaderVars.projection = camera.projection; 
    
    // Pass the params to shader
    m_shader->setShaderVariables(&m_shaderVars);
}

// TODO: * Loading scene file dynamically
void BasicApp::run()
{
    while (bRunning)
    {
        /* Tick */
        update();

        /* Render*/
        beginFrame();
        render();
        endFrame();
    }
}

void BasicApp::beginFrame()
{
    gEngine->getRenderer()->clearBackBuffer();
    
    // ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void BasicApp::render()
{
    gEngine->render(*m_scenes[0]);

    /* ImGui */
    ImGui::Begin("Transform");
    Entity* e = &m_scenes[currentScene]->entities[entityOnFocusIdx];
    gEngine->displayFloat3("Translation", e->xform.translation);
    gEngine->displayFloat3("Scale", e->xform.scale, true);
    gEngine->displaySliderFloat("Scale", &e->xform.scale.x, 0.0f, 10.f);
    gEngine->displaySliderFloat("ka", &m_shaderVars.kAmbient, 0.0f, 1.0f);
    gEngine->displaySliderFloat("kd", &m_shaderVars.kDiffuse, 0.0f, 1.0f);
    gEngine->displaySliderFloat("ks", &m_shaderVars.kSpecular, 0.0f, 7.0f);
    e->xform.scale.z = e->xform.scale.y = e->xform.scale.x;
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void BasicApp::endFrame()
{
    gEngine->getRenderer()->requestSwapBuffers();
}

void BasicApp::shutDown()
{
    // ImGui clean up
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    gEngine->shutDown();
    delete gEngine;
}

void BasicApp::orbitCamera(double deltaX, double deltaY)
{
    Camera& camera = m_scenes[currentScene]->mainCamera;

    /* Orbit around where the camera is looking at */
    float phi = deltaX * kCameraOrbitSpeed; 
    float theta = deltaY * kCameraOrbitSpeed;

    glm::vec3 p = camera.position - camera.lookAt;
    glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera.up);
    quat = glm::rotate(quat, -theta, camera.right);
    glm::mat4 rot = glm::toMat4(quat);
    glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
    camera.position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera.lookAt;
}

void BasicApp::rotateCamera(double deltaX, double deltaY)
{

}