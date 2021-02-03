#include <iostream>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "stb_image.h"

#include "Light.h"
#include "PbrApp.h"
#include "Shader.h"
#include "Mesh.h"


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
    /* Init engine instance */
    gEngine->setup({ appWindowWidth, appWindowHeight});
    bRunning = true;

    /* Setup inputs */
    gEngine->registerMouseCursorCallback(&Pbr::mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&Pbr::mouseButtonCallback);

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

    /* Init envmap */ 
    {
        m_cubeMesh = gEngine->getRenderer()->createMeshGroup("cubemapMesh");
        Mesh* mesh = new Mesh();
        mesh->setVertexBuffer(VertexBuffer::create(cubeVertices, sizeof(cubeVertices)));
        mesh->setNumVerts(sizeof(cubeVertices) / (sizeof(float) * 3));
        VertexBuffer* vb = mesh->getVertexBuffer();
        vb->pushVertexAttribute({{"Position"}, GL_FLOAT, 3, 0});
        mesh->initVertexAttributes();
        MeshManager::pushSubMesh(m_cubeMesh, mesh);

        // Camera
        m_genEnvmapCamera.position = glm::vec3(0.f, 0.f, 0.f);
        m_genEnvmapCamera.lookAt = glm::vec3(0.f, 0.f, -1.f);
        m_genEnvmapCamera.worldUp = glm::vec3(0.f, 1.f, 0.f);
        m_genEnvmapCamera.fov = 90.f;
        m_genEnvmapCamera.projection = glm::perspective(glm::radians(m_genEnvmapCamera.fov), 1.f, 0.1f, 100.f);
        CameraManager::updateCamera(m_genEnvmapCamera);

        // Environment map
        int w, h, numChannels;
        float* pixels = stbi_loadf("../../asset/cubemaps/grace-new.hdr", &w, &h, &numChannels, 0);

        glCreateTextures(GL_TEXTURE_2D, 1, &m_rawEnvmap);
        glTextureStorage2D(m_rawEnvmap, 1, GL_RGB16F, w, h);
        glTextureSubImage2D(m_rawEnvmap, 0, 0, 0, w, h, GL_RGB, GL_FLOAT, pixels);
        glBindTexture(GL_TEXTURE_2D, m_rawEnvmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Shaders 
        m_genEnvmapShader = new GenEnvmapShader("../../shader/shader_gen_cubemap.vs", "../../shader/shader_gen_cubemap.fs");
    }

    m_envmapShader = new EnvmapShader("../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
    m_genIrradianceShader = new GenIrradianceShader("../../shader/shader_diff_irradiance.vs", "../../shader/shader_diff_irradiance.fs");

    // Add lights into the scene
    {
        scene->dLights.push_back(
            DirectionalLight{
                glm::vec4{1.0f, 0.95f, 0.76f, 2.f}, 
                glm::vec4{0.0f, 0.0f, -1.0f, 0.f}
            }
        );

        scene->dLights.push_back(
            DirectionalLight{
                glm::vec4{1.0f, 0.95f, 0.76f, 2.f}, 
                glm::vec4{-0.5f, -0.3f, -1.0f, 0.f}
            }
        );

        scene->pLights.push_back(
            PointLight{
                glm::vec4{0.9f, 0.9f, 0.9f, 1.f}, 
                glm::vec4{0.4f, 1.5f, 2.4f, 1.f}
            });

        scene->pLights.push_back(
            PointLight{
                glm::vec4{0.9f, 0.9f, 0.9f, 6.f}, 
                glm::vec4{0.0f, 0.8f, -2.4f, 1.f}
            });
    }

    /* Displaying xform info */
    entityOnFocusIdx = 0;

    // Setup shaders
    m_shaderVars = { };
    /* Init shader variables */
    // TODO: Following should really be part of material
    m_shaderVars.kAmbient = 0.2f;
    m_shaderVars.kDiffuse = 1.0f;
    m_shaderVars.kSpecular = 1.0f;
    m_shaderVars.projection = m_scenes[currentScene]->mainCamera.projection;

    m_shader = new PbrShader("../../shader/shader_pbr.vs", "../../shader/shader_pbr.fs");

    // TODO: Implement GfxContext class that handles rendering states ?
    // gEngine->GfxContext->bindShader();
    gEngine->getRenderer()->registerShader(m_shader);

    gEngine->getRenderer()->enableDepthTest();
    gEngine->getRenderer()->setClearColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.f));
}

void PbrApp::beginFrame()
{
    gEngine->getRenderer()->clearBackBuffer();
    
    // ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

GLuint PbrApp::createCubemapFromEquirectMap(int w, int h)
{
    GLuint fbo, rbo, cubemap;
    glCreateFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    glCreateRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    const int kNumFaces = 6;
    glm::vec3 cameraTargets[] = {
        {1.f, 0.f, 0.f},   // Right
        {-1.f, 0.f, 0.f},  // Left
        {0.f, 1.f, 0.f},   // Up
        {0.f, -1.f, 0.f},  // Down
        {0.f, 0.f, 1.f},   // Front
        {0.f, 0.f, -1.f},  // Back
    }; 

    // TODO: (Min): Need to figure out why we need to flip the y-axis 
    // I thought they should just be vec3(0.f, 1.f, 0.f)
    // Referrence: https://www.khronos.org/opengl/wiki/Cubemap_Texture
    glm::vec3 worldUps[] = {
        {0.f, -1.f, 0.f},   // Right
        {0.f, -1.f, 0.f},   // Left
        {0.f, 0.f, 1.f},    // Up
        {0.f, 0.f, -1.f},   // Down
        {0.f, -1.f, 0.f},   // Forward
        {0.f, -1.f, 0.f},   // Back
    };

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    for (int f = 0; f < 6; f++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    EnvmapShaderVars vars = { };
    glDisable(GL_DEPTH_TEST);

    // Since we are rendering to a framebuffer, we need to configure the viewport 
    // to prevent the texture being stretched to fit the framebuffer's dimension
    glViewport(0, 0, w, h);
    for (int f = 0; f < kNumFaces; f++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, cubemap, 0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        }

        // Update view matrix
        m_genEnvmapCamera.lookAt = cameraTargets[f];
        m_genEnvmapCamera.worldUp = worldUps[f];
        CameraManager::updateCamera(m_genEnvmapCamera);
        vars.view = m_genEnvmapCamera.view;
        vars.projection = m_genEnvmapCamera.projection;
        vars.envmap = m_rawEnvmap; 
        m_genEnvmapShader->setShaderVariables(&vars);
        m_genEnvmapShader->prePass();

        gEngine->getRenderer()->drawMesh(*m_cubeMesh);
    }
    // Recover the viewport dimensions
    glViewport(0, 0, gEngine->getWindow().width, gEngine->getWindow().height);
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  
    return cubemap;
}

GLuint PbrApp::createDiffuseIrradianceMap(int w, int h)
{
    GLuint fbo, rbo, cubemap;
    Camera camera = { };
    camera.projection = glm::perspective(45.0f, 1.0f, 0.1f, 100.f); 
    glCreateFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    
    glCreateRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    const int kNumFaces = 6;
    glm::vec3 cameraTargets[] = {
        {1.f, 0.f, 0.f},   // Right
        {-1.f, 0.f, 0.f},  // Left
        {0.f, 1.f, 0.f},   // Up
        {0.f, -1.f, 0.f},  // Down
        {0.f, 0.f, 1.f},   // Front
        {0.f, 0.f, -1.f},  // Back
    }; 

    // TODO: (Min): Need to figure out why we need to flip the y-axis 
    // I thought they should just be vec3(0.f, 1.f, 0.f)
    // Referrence: https://www.khronos.org/opengl/wiki/Cubemap_Texture
    glm::vec3 worldUps[] = {
        {0.f, -1.f, 0.f},   // Right
        {0.f, -1.f, 0.f},   // Left
        {0.f, 0.f, 1.f},    // Up
        {0.f, 0.f, -1.f},   // Down
        {0.f, -1.f, 0.f},   // Forward
        {0.f, -1.f, 0.f},   // Back
    };

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
    for (int f = 0; f < 6; f++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    EnvmapShaderVars vars = { };
    glDisable(GL_DEPTH_TEST);

    // Since we are rendering to a framebuffer, we need to configure the viewport 
    // to prevent the texture being stretched to fit the framebuffer's dimension
    glViewport(0, 0, w, h);
    for (int f = 0; f < kNumFaces; f++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, cubemap, 0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        }

        // Update view matrix
        camera.lookAt = cameraTargets[f];
        camera.worldUp = worldUps[f];
        CameraManager::updateCamera(camera);
        vars.view = camera.view;
        vars.projection = camera.projection;
        vars.envmap = m_envmap; 
        m_genIrradianceShader->setShaderVariables(&vars);
        m_genIrradianceShader->prePass();
        gEngine->getRenderer()->drawMesh(*m_cubeMesh);
    }
    // Recover the viewport dimensions
    glViewport(0, 0, gEngine->getWindow().width, gEngine->getWindow().height);
    glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  
    return cubemap;
}

void PbrApp::run()
{
    // Generate envmaps from a hdri equirectangular map
    {
        m_envmap = gEngine->getRenderer()->createCubemapFromTexture(m_genEnvmapShader, m_rawEnvmap, m_cubeMesh, 
                                                                    gEngine->getWindow().width, gEngine->getWindow().height,
                                                                    1024, 1024);
        m_diffuseIrradianceMap = gEngine->getRenderer()->createCubemapFromTexture(m_genIrradianceShader, m_envmap, m_cubeMesh,
                                                                gEngine->getWindow().width, gEngine->getWindow().height,
                                                                1024, 1024);
        glDepthFunc(GL_LEQUAL);
    }

    // Generate diffuse irradiance map from envmap
    {
        EnvmapShaderVars irradianceVars = { };
        irradianceVars.envmap = m_envmap;

        m_genIrradianceShader->setShaderVariables(&irradianceVars);
        m_genIrradianceShader->prePass();
        gEngine->getRenderer()->drawMesh(*m_cubeMesh);

        // Sync to make sure writes happened in last draw call is visible
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Read out vertex data for debug lines
        void* baseAddr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        memcpy(m_sampleVertex, baseAddr, sizeof(float) * 3 * 16);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    Camera& camera = m_scenes[currentScene]->mainCamera;
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

void PbrApp::update()
{
    Camera& camera = m_scenes[currentScene]->mainCamera;
    CameraManager::updateCamera(camera);

    m_shaderVars.view = camera.view; 
    m_shaderVars.projection = camera.projection; 
    m_shaderVars.pLights = &m_scenes[currentScene]->pLights[0];
    m_shaderVars.numPointLights = m_scenes[currentScene]->pLights.size();
    m_shaderVars.dLights = &m_scenes[currentScene]->dLights[0];
    m_shaderVars.numDirLights = m_scenes[currentScene]->dLights.size();
    
    // Pass the params to shader
    m_shader->setShaderVariables(&m_shaderVars);

    EnvmapShaderVars vars = { };
    vars.view = camera.view;
    vars.projection = camera.projection;
    vars.envmap = m_diffuseIrradianceMap;
    m_envmapShader->setShaderVariables(&vars);
}

void PbrApp::render()
{
    gEngine->render(*m_scenes[0]);

    // Envmap pass
    {
        m_envmapShader->prePass();
        gEngine->getRenderer()->drawMesh(*m_cubeMesh);
    }

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

void PbrApp::endFrame()
{
    gEngine->getRenderer()->requestSwapBuffers();
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