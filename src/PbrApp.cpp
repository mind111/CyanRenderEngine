#include <iostream>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "stb_image.h"

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
    /* Init engine instance */
    gEngine->setup({ appWindowWidth, appWindowHeight});
    bRunning = true;

    /* Setup inputs */
    gEngine->registerMouseCursorCallback(&Pbr::mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&Pbr::mouseButtonCallback);

    /* Setup scenes */
    Scene* scene = gEngine->loadScene("../../scene/default_scene/scene_config.json");
    m_scenes.push_back(scene);
    currentScene = 0;

    /* Test */
    Shader* pbrShader = Cyan::createShader("../../shader/shader_pbr.vs" , "../../shader/shader_pbr.fs");
    Material* helmetMatl = Cyan::createMaterial(pbrShader);
    {
        for (int i = 0; i < 6; i++)
        {
            char samplerName[64];
            std::sprintf(samplerName, "diffuseMaps[%d]", i);
            Uniform* u_albedo = Cyan::createUniform(samplerName, Uniform::UniformType::u_sampler2D);
            helmetMatl->pushUniform(u_albedo);
        }

        for (int i = 0; i < 2; i++)
        {
            char samplerName[64];
            std::sprintf(samplerName, "emissionMaps[%d]", i);
            Uniform* u_emission = Cyan::createUniform(samplerName, Uniform::UniformType::u_sampler2D);
            helmetMatl->pushUniform(u_emission);
        }

        Uniform* u_normal = Cyan::createUniform("normalMap", Uniform::UniformType::u_sampler2D);
        Uniform* u_roughness = Cyan::createUniform("roughnessMap", Uniform::UniformType::u_sampler2D);
        Uniform* u_normal = Cyan::createUniform("aoMap", Uniform::UniformType::u_sampler2D);

        helmetMatl->pushTexture("diffuseMaps[0]", gEngine->m_renderer->findTexture("helmet_diffuse"));
        helmetMatl->pushTexture("normalMap", gEngine->m_renderer->findTexture("helmet_nm"));
        helmetMatl->pushTexture("roughnessMap", gEngine->m_renderer->findTexture("helmet_roughness"));
        helmetMatl->pushTexture("aoMap", gEngine->m_renderer->findTexture("helmet_ao"));
    }

    Uniform* u_numPointLights = Cyan::createUniform("numPointLights", Uniform::UniformType::u_int);
    Uniform* u_numDirLights = Cyan::createUniform("numDirLights", Uniform::UniformType::u_int);

    pbrShader->pushUniform(u_numPointLights);

    /* Init envmap */ 
    {
        m_cubeMesh = gEngine->m_renderer->createCubeMesh("cubemapMesh");

        // Camera
        m_genEnvmapCamera.position = glm::vec3(0.f, 0.f, 0.f);
        m_genEnvmapCamera.lookAt = glm::vec3(0.f, 0.f, -1.f);
        m_genEnvmapCamera.worldUp = glm::vec3(0.f, 1.f, 0.f);
        m_genEnvmapCamera.fov = 90.f;
        m_genEnvmapCamera.projection = glm::perspective(glm::radians(m_genEnvmapCamera.fov), 1.f, 0.1f, 100.f);
        CameraManager::updateCamera(m_genEnvmapCamera);

        // Environment map
        m_rawEnvmap = CyanRenderer::Toolkit::createHDRTextureFromFile("../../asset/cubemaps/grace-new.hdr", TextureType::TEX_2D, TexFilterType::LINEAR); 

        // Shaders 
        m_genEnvmapShader = new GenEnvmapShader("../../shader/shader_gen_cubemap.vs", "../../shader/shader_gen_cubemap.fs");
        m_envmapShader = new EnvmapShader("../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
        m_genIrradianceShader = new GenIrradianceShader("../../shader/shader_diff_irradiance.vs", "../../shader/shader_diff_irradiance.fs");
    }

    // Add lights into the scene
    {
        SceneManager::createDirectionalLight(*scene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(0.f, 0.f, -1.f), 2.f);
        SceneManager::createDirectionalLight(*scene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(-0.5f, -0.3f, -1.f), 2.f);
        SceneManager::createPointLight(*scene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.4f, 1.5f, 2.4f), 1.f);
        SceneManager::createPointLight(*scene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.0f, 0.8f, -2.4f), 1.f);
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

    gEngine->m_renderer->registerShader(m_shader);

    gEngine->m_renderer->enableDepthTest();
    gEngine->m_renderer->setClearColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.f));
}

void PbrApp::beginFrame()
{
    gEngine->m_renderer->clearBackBuffer();
    
    // ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}


void PbrApp::run()
{
    // Generate envmaps from a hdri equirectangular map
    {
        m_envmap = gEngine->getRenderer()->createCubemapFromTexture(m_genEnvmapShader, m_rawEnvmap, m_cubeMesh, 
                                                                    gEngine->getWindow().width, gEngine->getWindow().height,
                                                                    1024, 1024);

        // Generate diffuse irradiace map from envmap
        m_diffuseIrradianceMap = gEngine->getRenderer()->createCubemapFromTexture(m_genIrradianceShader, m_envmap, m_cubeMesh,
                                                                gEngine->getWindow().width, gEngine->getWindow().height,
                                                                256, 256);
        glDepthFunc(GL_LEQUAL);
    }

    m_shaderVars.envmap = &m_envmap;
    m_shaderVars.diffuseIrradianceMap = &m_diffuseIrradianceMap;

    {
#if DRAW_DEBUG_LINES 
        EnvmapShaderVars irradianceVars = { };
        irradianceVars.envmap = m_envmap;

        m_genIrradianceShader->setShaderVariables(&irradianceVars);
        m_genIrradianceShader->prePass();
        gEngine->getRenderer()->drawMesh(*m_cubeMesh);

        // Sync to make sure writes happened in last draw call is visible
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Read out vertex data for debug lines
        void* baseAddr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
        memcpy(m_sampleVertex, baseAddr, sizeof(float) * 4 * 65 * 2);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        // Setup data for debug lines
        m_lineShader = new LineShader("../../shader/shader_line.vs", "../../shader/shader_line.fs");
        glCreateVertexArrays(1, &m_linesVao);
        glBindVertexArray(m_linesVao);
        glCreateBuffers(1, &m_linesVbo);
        glNamedBufferData(m_linesVbo, sizeof(m_sampleVertex), &m_sampleVertex, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, m_linesVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif
    }

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
    // TODO: How to update uniforms ...?
    ctx->setUniform("numPointLights", 10);
    ctx->setUniform("numDirLights", 6);


    Camera& camera = m_scenes[currentScene]->mainCamera;
    CameraManager::updateCamera(camera);

    m_shader->m_vars.view = camera.view; 
    m_shader->m_vars.projection = camera.projection; 
    m_shader->m_vars.pLights = &m_scenes[currentScene]->pLights[0];
    m_shader->m_vars.numPointLights = m_scenes[currentScene]->pLights.size();
    m_shader->m_vars.dLights = &m_scenes[currentScene]->dLights[0];
    m_shader->m_vars.numDirLights = m_scenes[currentScene]->dLights.size();

    m_envmapShader->m_vars.view = camera.view;
    m_envmapShader->m_vars.projection = camera.projection;
    m_envmapShader->m_vars.envmap = m_diffuseIrradianceMap;

#if DRAW_DEBUG_LINES
    LineShaderVars lineShaderVars = { };
    lineShaderVars.model = glm::mat4(1.f);
    lineShaderVars.view = camera.view;
    lineShaderVars.projection = camera.projection;
    m_lineShader->setShaderVariables(&lineShaderVars);
#endif
}

void PbrApp::render()
{
    // Draw entities in the scene
    gEngine->render(*m_scenes[0]);

    // Envmap pass
    {
        m_envmapShader->prePass();
        gEngine->getRenderer()->drawMesh(*m_cubeMesh);
    }

    // UI
    {
        Entity* e = &m_scenes[currentScene]->entities[entityOnFocusIdx];
        ImGui::Begin("Transform");
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

#if DRAW_DEBUG_LINES
    // Draw debug lines
    {
        m_lineShader->prePass();
        glBindVertexArray(m_linesVao);
        glDrawArrays(GL_LINES, 0, 130);
    }
#endif
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