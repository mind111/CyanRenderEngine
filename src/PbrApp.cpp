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
    // init shaders
    m_pbrShader = Cyan::createShader("../../shader/shader_pbr.vs" , "../../shader/shader_pbr.fs");
    // create uniforms
    // TODO: automate creating shader uniforms and material uniforms 
    u_numPointLights = Cyan::createShaderUniform(m_pbrShader, "numPointLights", Uniform::Type::u_int);
    u_numDirLights = Cyan::createShaderUniform(m_pbrShader, "numDirLights", Uniform::Type::u_int);
    u_kDiffuse = Cyan::createShaderUniform(m_pbrShader, "kDiffuse", Uniform::Type::u_float);
    u_kSpecular = Cyan::createShaderUniform(m_pbrShader, "kSpecular", Uniform::Type::u_float);
    u_cameraProjection = Cyan::createShaderUniform(m_pbrShader, "projection", Uniform::Type::u_mat4);
    u_cameraView = Cyan::createShaderUniform(m_pbrShader, "view", Uniform::Type::u_mat4);
    u_hasNormalMap = Cyan::createShaderUniform(m_pbrShader, "hasNormalMap", Uniform::Type::u_float); 
    u_hasAoMap = Cyan::createShaderUniform(m_pbrShader, "hasAoMap", Uniform::Type::u_float); 
    Cyan::setUniform(u_hasNormalMap, 1.f);
    Cyan::setUniform(u_hasAoMap, 1.f);
    m_dirLightsBuffer = Cyan::createRegularBuffer("dirLightsData", m_pbrShader, 1, sizeof(DirectionalLight) * 10);
    m_pointLightsBuffer = Cyan::createRegularBuffer("pointLightsData", m_pbrShader, 2, sizeof(PointLight) * 10);
    // init materials
    m_helmetMatl = Cyan::createMaterial(m_pbrShader);
    for (int i = 0; i < 6; i++)
    {
        char samplerName[64];
        std::sprintf(samplerName, "diffuseMaps[%d]", i);
        Cyan::createMaterialUniform(m_helmetMatl, samplerName, Uniform::Type::u_sampler2D);
    }
    for (int i = 0; i < 2; i++)
    {
        char samplerName[64];
        std::sprintf(samplerName, "emissionMaps[%d]", i);
        Cyan::createMaterialUniform(m_helmetMatl, samplerName, Uniform::Type::u_sampler2D);
    }
    Cyan::createMaterialUniform(m_helmetMatl, "normalMap", Uniform::Type::u_sampler2D);
    Cyan::createMaterialUniform(m_helmetMatl, "roughnessMap", Uniform::Type::u_sampler2D);
    Cyan::createMaterialUniform(m_helmetMatl, "aoMap", Uniform::Type::u_sampler2D);
    m_helmetMatl->addTexture("diffuseMaps[0]", Cyan::getTexture("helmet_diffuse"));
    m_helmetMatl->addTexture("normalMap", Cyan::getTexture("helmet_nm"));
    m_helmetMatl->addTexture("roughnessMap", Cyan::getTexture("helmet_roughness"));
    m_helmetMatl->addTexture("aoMap", Cyan::getTexture("helmet_ao"));

    Mesh* helmetMesh = Cyan::getMesh("helmet_mesh");
    helmetMesh->setMaterial(0, m_helmetMatl);

    // Envmap related
    {
        m_envmapShader = Cyan::createShader("../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
        m_envmapMatl = Cyan::createMaterial(m_envmapShader); 
        Cyan::createMaterialUniform(m_envmapMatl, "envmapSampler", Uniform::Type::u_samplerCube);
        m_envmapShader->bindUniform(u_cameraProjection);
        m_envmapShader->bindUniform(u_cameraView);
        // create cubemap mesh
        Cyan::Toolkit::createCubeMesh("cubemapMesh");
        // Generate cubemap from a hdri equirectangular map
        m_envmap = Cyan::Toolkit::loadEquirectangularMap("grace-new", "../../asset/cubemaps/grace-new.hdr", true);
        m_envmapMatl->addTexture("envmapSampler", m_envmap);
        Cyan::getMesh("cubemapMesh")->setMaterial(0, m_envmapMatl);
        glDepthFunc(GL_LEQUAL);
        // Generate diffuse irradiace map from envmap
        // m_diffuseIrradianceMap = Cyan::Toolkit::generateDiffsueIrradianceMap("diffuse_irradiance_map", m_envmap, true);
        // m_helmetMatl->addTexture("envmap", m_envmap);
        // createMaterialUniform("diffuseIrradianceMap", Uniform::Type::u_samplerCube, m_helmetMatl);
        // m_helmetMatl->addTexture("diffuseIrradianceMap", m_diffuseIrradianceMap);
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

    Cyan::setUniform(u_kDiffuse, 1.0f);
    Cyan::setUniform(u_kSpecular, 1.0f);

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
    // {
    //     using Cyan::Texture;
    //     using Cyan::Mesh;

    //     const u32 kViewportWidth = 1024;
    //     const u32 kViewportHeight = 1024;

    //     // Create textures
    //     equirectMap = Cyan::createTextureHDR("equirectMap", "../../asset/cubemaps/grace-new.hdr");
    //     envmap_dbg = Cyan::createTextureHDR("envmap", kViewportWidth, kViewportHeight, Texture::ColorFormat::R16G16B16, Texture::Type::TEX_CUBEMAP);
    //     // Create shaders and uniforms
    //     shader_dbg = Cyan::createShader("../../shader/shader_gen_cubemap.vs", "../../shader/shader_gen_cubemap.fs");
    //     u_projection_dbg = Cyan::createUniform("projection", Uniform::Type::u_mat4);
    //     u_view_dbg = Cyan::createUniform("view", Uniform::Type::u_mat4);
    //     u_envmapSampler = Cyan::createUniform("rawEnvmapSampler", Uniform::Type::u_sampler2D);
    //     shader_dbg->bindUniform(u_projection_dbg);
    //     shader_dbg->bindUniform(u_view_dbg);
    //     shader_dbg->bindUniform(u_envmapSampler);
    //     rt_dbg = Cyan::createRenderTarget(1024, 1024);
    //     rt_dbg->attachColorBuffer(envmap_dbg);
    // }

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
    Camera& camera = m_scenes[currentScene]->mainCamera;
    CameraManager::updateCamera(camera);
    Cyan::setUniform(u_numPointLights, (u32)m_scenes[currentScene]->pLights.size());
    Cyan::setUniform(u_numDirLights, (u32)m_scenes[currentScene]->dLights.size());
    Cyan::setUniform(u_cameraView, (void*)&camera.view[0]);
    Cyan::setUniform(u_cameraProjection, (void*)&camera.projection[0]);
    Cyan::setBuffer(m_pointLightsBuffer, m_scenes[currentScene]->pLights.data(), sizeofVector(m_scenes[currentScene]->pLights));
    Cyan::setBuffer(m_dirLightsBuffer, m_scenes[currentScene]->dLights.data(), sizeofVector(m_scenes[currentScene]->dLights));
}

void PbrApp::render()
{
// {
//     using Cyan::Texture;
//     using Cyan::Mesh;

//     const u32 kViewportWidth = 1024;
//     const u32 kViewportHeight = 1024;

//     Camera camera = { };
//     camera.position = glm::vec3(0.f);
//     camera.projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.f); 

//     glm::vec3 cameraTargets[] = {
//         {1.f, 0.f, 0.f},   // Right
//         {-1.f, 0.f, 0.f},  // Left
//         {0.f, 1.f, 0.f},   // Up
//         {0.f, -1.f, 0.f},  // Down
//         {0.f, 0.f, 1.f},   // Front
//         {0.f, 0.f, -1.f},  // Back
//     }; 

//     // TODO: (Min): Need to figure out why we need to flip the y-axis 
//     // I thought they should just be vec3(0.f, 1.f, 0.f)
//     // Referrence: https://www.khronos.org/opengl/wiki/Cubemap_Texture
//     glm::vec3 worldUps[] = {
//         {0.f, -1.f, 0.f},   // Right
//         {0.f, -1.f, 0.f},   // Left
//         {0.f, 0.f, 1.f},    // Up
//         {0.f, 0.f, -1.f},   // Down
//         {0.f, -1.f, 0.f},   // Forward
//         {0.f, -1.f, 0.f},   // Back
//     };

//     auto s_gfxc = Cyan::getCurrentGfxCtx();
//     Mesh* cubeMesh = Cyan::getMesh("cubemapMesh");
//     // Cache viewport config
//     glm::vec4 origViewport = Cyan::getCurrentGfxCtx()->m_viewport;
//     for (u32 f = 0; f < 6u; f++)
//     {
//         // Update view matrix
//         camera.lookAt = cameraTargets[f];
//         camera.worldUp = worldUps[f];
//         CameraManager::updateCamera(camera);
//         // Update uniform
//         Cyan::setUniform(u_projection_dbg, &camera.projection);
//         Cyan::setUniform(u_view_dbg, &camera.view);

//         s_gfxc->setDepthControl(Cyan::DepthControl::kDisable);
//         // Since we are rendering to a framebuffer, we need to configure the viewport 
//         // to prevent the texture being stretched to fit the framebuffer's dimension
//         s_gfxc->setViewport(0, 0, kViewportWidth, kViewportHeight);
//         s_gfxc->setRenderTarget(rt_dbg, 1 << f);
//         s_gfxc->setShader(shader_dbg);
//         s_gfxc->setUniform(u_projection_dbg);
//         s_gfxc->setUniform(u_view_dbg);
//         s_gfxc->setSampler(u_envmapSampler, 0);
//         s_gfxc->setTexture(equirectMap, 0);
//         s_gfxc->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
//         s_gfxc->setVertexArray(cubeMesh->m_subMeshes[0]->m_vertexArray);

//         s_gfxc->drawIndex(cubeMesh->m_subMeshes[0]->m_numVerts, 0);
//     }
//     // Recover the viewport dimensions
//     s_gfxc->setViewport(origViewport.x, origViewport.y, origViewport.z, origViewport.w);
//     s_gfxc->setDepthControl(Cyan::DepthControl::kEnable);
//     m_envmapMatl->addTexture("envmapSampler", envmap_dbg);
//     Cyan::getMesh("cubemapMesh")->setMaterial(0, m_envmapMatl);
//     s_gfxc->reset();
// }

    Cyan::Renderer* renderer = gEngine->getRenderer();
    // draw entities in the scene
    renderer->render(m_scenes[0]);
    // envmap pass
    renderer->drawMesh(Cyan::getMesh("cubemapMesh"));
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