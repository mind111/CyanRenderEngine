#include <iostream>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "stb_image.h"

#include "CyanAPI.h"
#include "Light.h"
#include "PbrSpheres.h"
#include "Shader.h"
#include "Mesh.h"

#define DRAW_DEBUG_LINES 0

/* Constants */
// In radians per pixel 

static float kCameraOrbitSpeed = 0.005f;
static float kCameraRotateSpeed = 0.005f;

namespace PbrSpheres
{

    void mouseCursorCallback(double deltaX, double deltaY)
    {
        PbrSpheresSample* app = PbrSpheresSample::get();
        app->bOrbit ? app->orbitCamera(deltaX, deltaY) : app->rotateCamera(deltaX, deltaY);
    }

    void mouseButtonCallback(int button, int action)
    {
        PbrSpheresSample* app = PbrSpheresSample::get();
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

static PbrSpheresSample* gApp = nullptr;

PbrSpheresSample* PbrSpheresSample::get()
{
    if (gApp)
        return gApp;
    return nullptr;
}

PbrSpheresSample::PbrSpheresSample()
{
    bOrbit = false;
    gApp = this;
}

void PbrSpheresSample::init(int appWindowWidth, int appWindowHeight)
{
    using Cyan::Material;
    using Cyan::Mesh;
    // init engine
    gEngine->init({ appWindowWidth, appWindowHeight});
    bRunning = true;

    // setup input control
    gEngine->registerMouseCursorCallback(&PbrSpheres::mouseCursorCallback);
    gEngine->registerMouseButtonCallback(&PbrSpheres::mouseButtonCallback);

    // setup scenes
    m_scene = Cyan::createScene("../../scene/default_scene/scene_spheres.json");
    m_scene->mainCamera.projection = glm::perspective(glm::radians(m_scene->mainCamera.fov), (float)(gEngine->getWindow().width) / gEngine->getWindow().height, m_scene->mainCamera.n, m_scene->mainCamera.f);

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

    // envmap related
    m_envmapShader = Cyan::createShader("EnvMapShader", "../../shader/shader_envmap.vs", "../../shader/shader_envmap.fs");
    m_envmapMatl = Cyan::createMaterial(m_envmapShader)->createInstance(); 
    m_envmap = Cyan::Toolkit::loadEquirectangularMap("grace-new", "../../asset/cubemaps/grace-new.hdr", true);
    m_envmapMatl->bindTexture("envmapSampler", m_envmap);
    glDepthFunc(GL_LEQUAL);

    // Generate cubemap from a hdri equirectangular map
    Entity* envmapEntity = SceneManager::createEntity(m_scene, "cubemapMesh");
    envmapEntity->m_meshInstance->setMaterial(0, m_envmapMatl);

    // Generate diffuse irradiace map from envmap
    m_iblAssets = { };
    m_iblAssets.m_diffuse = Cyan::Toolkit::prefilterEnvMapDiffuse("diffuse_irradiance_map", m_envmap, true);
    m_iblAssets.m_specular = Cyan::Toolkit::prefilterEnvmapSpecular(m_envmap);
    m_iblAssets.m_brdfIntegral = Cyan::Toolkit::generateBrdfLUT();

    // init materials
    Cyan::Material* materialType = Cyan::createMaterial(m_pbrShader);
    Cyan::Texture* m_sphereAlbedo = Cyan::Toolkit::createFlatColorTexture("albedo", 1024u, 1024u, glm::vec4(0.4f, 0.4f, 0.4f, 1.f));
    // 6 x 6 grid of spheres
    u32 numRows = 6u;
    u32 numCols = 6u;
    for (u32 row = 0u; row < numRows; ++row)
    {
        for (u32 col = 0u; col < numCols; ++col)
        {
            u32 idx = row * numCols + col;
            // create entity
            glm::vec3 topLeft(-1.5f, 1.5f, -1.8f);
            Transform transform = {
                topLeft + glm::vec3(0.5f * col, -0.5f * row, 0.f),
                glm::quat(1.f, glm::vec3(0.f)), // identity quaternion
                glm::vec3(.2f)
            };
            Entity* entity = SceneManager::createEntity(m_scene, "sphere_mesh", transform);

            // create material
            m_sphereMatls[idx] = materialType->createInstance();
            m_sphereMatls[idx]->bindTexture("diffuseMaps[0]", m_sphereAlbedo);
            m_sphereMatls[idx]->bindTexture("envmap", m_envmap);
            m_sphereMatls[idx]->bindTexture("irradianceDiffuse", m_iblAssets.m_diffuse);
            m_sphereMatls[idx]->bindTexture("irradianceSpecular", m_iblAssets.m_specular);
            m_sphereMatls[idx]->bindTexture("brdfIntegral", m_iblAssets.m_brdfIntegral);
            m_sphereMatls[idx]->set("hasAoMap", 0.f);
            m_sphereMatls[idx]->set("hasNormalMap", 0.f);
            m_sphereMatls[idx]->set("hasRoughnessMap", 0.f);
            m_sphereMatls[idx]->set("kDiffuse", 1.0f);
            m_sphereMatls[idx]->set("kSpecular", 1.0f);
            // roughness increases in horizontal direction
            m_sphereMatls[idx]->set("uniformRoughness", 0.2f * col);
            // metallic increases in vertical direction
            m_sphereMatls[idx]->set("uniformMetallic", 0.2f * row);

            entity->m_meshInstance->setMaterial(0, m_sphereMatls[idx]);
        }
    }

    // add lights into the scene
    SceneManager::createDirectionalLight(*m_scene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(0.f, 0.f, -1.f), 2.f);
    SceneManager::createDirectionalLight(*m_scene, glm::vec3(1.0, 0.95f, 0.76f), glm::vec3(-0.5f, -0.3f, -1.f), 2.f);
    SceneManager::createPointLight(*m_scene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.4f, 1.5f, 2.4f), 1.f);
    SceneManager::createPointLight(*m_scene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.0f, 0.8f, -2.4f), 1.f);

    // misc
    Cyan::getCurrentGfxCtx()->setClearColor(glm::vec4(0.2f, 0.2f, 0.2f, 1.f));
}

void PbrSpheresSample::beginFrame()
{
    Cyan::getCurrentGfxCtx()->clear();
    Cyan::getCurrentGfxCtx()->setViewport(0, 0, gEngine->getWindow().width, gEngine->getWindow().height);
}

void PbrSpheresSample::run()
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

void PbrSpheresSample::update()
{
    gEngine->processInput();
}


void PbrSpheresSample::render()
{
    Camera& camera = m_scene->mainCamera;
    CameraManager::updateCamera(camera);
    u32 numRows = 6u;
    u32 numCols = 6u;
    for (u32 row = 0u; row < numRows; ++row)
    {
        for (u32 col = 0u; col < numCols; ++col)
        {
            u32 idx = row * numCols + col;
            m_sphereMatls[idx]->set("numDirLights", (u32)m_scene->dLights.size());
            m_sphereMatls[idx]->set("numPointLights", (u32)m_scene->pLights.size());
        }
    }

    Cyan::setUniform(u_cameraView, (void*)&camera.view[0]);
    Cyan::setUniform(u_cameraProjection, (void*)&camera.projection[0]);
    if (!m_scene->pLights.empty())
        Cyan::setBuffer(m_pointLightsBuffer, m_scene->pLights.data(), sizeofVector(m_scene->pLights));
    if (!m_scene->dLights.empty())
        Cyan::setBuffer(m_dirLightsBuffer, m_scene->dLights.data(), sizeofVector(m_scene->dLights));

    Cyan::Renderer* renderer = gEngine->getRenderer();
    // draw entities in the scene
    renderer->renderScene(m_scene);
    renderer->endFrame();
}

void PbrSpheresSample::endFrame()
{

}

void PbrSpheresSample::shutDown()
{
    gEngine->shutDown();
    delete gEngine;
}

void PbrSpheresSample::orbitCamera(double deltaX, double deltaY)
{
    Camera& camera = m_scene->mainCamera;
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

void PbrSpheresSample::rotateCamera(double deltaX, double deltaY)
{

}