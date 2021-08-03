#include <iostream>
#include <queue>
#include <functional>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "stb_image.h"

#include "CyanUI.h"
#include "CyanAPI.h"
#include "Light.h"
#include "PbrApp.h"
#include "Shader.h"
#include "Mesh.h"

/*
    * add key light, fill light, and backlight
    * add mesh for each rotating point light
    * particle system; particle rendering
    * grass rendering
    * procedural sky & clouds
    * look into ue4's skylight implementation
    * implement bloom
    * toon shading
    * normalized blinn phong shading
    * add more entities into the scene
    * scene graph, animation
    * select entity via ui; highlight selected entities in the scene
    * saving the scene and assets as binaries (serialization)
*/


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

void PbrApp::initHelmetScene()
{
    // setup scenes
    Scene* helmetScene = Cyan::createScene("helmet_scene", "../../scene/default_scene/scene_config.json");
    helmetScene->mainCamera.projection = glm::perspective(glm::radians(helmetScene->mainCamera.fov), (float)(gEngine->getWindow().width) / gEngine->getWindow().height, helmetScene->mainCamera.n, helmetScene->mainCamera.f);

    // TODO:
    Cyan::Mesh* uvSphereMesh = Cyan::getMesh("sphere_mesh");
    // Entity* envMapEntity = SceneManager::createEntity(helmetScene, "Envmap", "cubemapMesh", Transform(), false);
    // envMapEntity->m_meshInstance->setMaterial(0, m_envmapMatl);

    Cyan::Mesh* terreinMesh = Cyan::AssetGen::createTerrain(40.f, 40.f);
    m_terrainMatl = Cyan::createMaterial(m_pbrShader)->createInstance();
    Transform terrainTransform = Transform();
    terrainTransform.m_translate = glm::vec3(0.f, -1.5f, 0.f);
    Entity* terrainEntity = SceneManager::createEntity(helmetScene, "Terrain", "terrain_mesh", terrainTransform, true);
    SceneManager::createSceneNode(helmetScene, nullptr, terrainEntity);

    // TODO: use this sphere to debug bloom
    Transform sphereTransform = Transform();
    sphereTransform.m_scale = glm::vec3(0.5, 0.5f, 0.5f);
    sphereTransform.m_translate = glm::vec3(-1.0f, 0.f, -1.f);
    // Entity* sphereEntity = SceneManager::createEntity(helmetScene, "BloomSphere", "sphere_mesh", sphereTransform, true);
    // SceneManager::createSceneNode(helmetScene, nullptr, sphereEntity);
    // Cyan::MaterialInstance* sphereMatl = Cyan::createMaterial(m_pbrShader)->createInstance();
    // Cyan::Texture* sphereAlbedo = Cyan::Toolkit::createFlatColorTexture("sphere_albedo", 1024u, 1024u, glm::vec4(0.9f, 0.5f, 0.5f, 1.f));
    // sphereMatl->bindTexture("diffuseMaps[0]", sphereAlbedo);
    // sphereMatl->set("hasAoMap", 0.f);
    // sphereMatl->set("hasNormalMap", 0.f);

    // m_terrainMatl->bindTexture("diffuseMaps[0]", Cyan::getTexture("grass_albedo"));
    // m_terrainMatl->bindTexture("normalMap", Cyan::getTexture("grass_normal"));
    // m_terrainMatl->bindTexture("aoMap", Cyan::getTexture("grass_occlusion"));
    // m_terrainMatl->bindTexture("roughnessMap", Cyan::getTexture("grass_roughness"));
    Cyan::Texture* terrainAlbedo = Cyan::Toolkit::createFlatColorTexture("terrain_albedo", 1024u, 1024u, glm::vec4(0.9f, 0.9f, 0.6f, 1.f));
    m_terrainMatl->bindTexture("diffuseMaps[0]", terrainAlbedo);
    m_terrainMatl->bindTexture("envmap", m_envmap);
    m_terrainMatl->bindBuffer("dirLightsData", helmetScene->m_dirLightsBuffer);
    m_terrainMatl->bindBuffer("pointLightsData", helmetScene->m_pointLightsBuffer);
    m_terrainMatl->set("hasAoMap", 0.f);
    m_terrainMatl->set("hasNormalMap", 0.f);
    m_terrainMatl->set("kDiffuse", 1.0f);
    m_terrainMatl->set("kSpecular", 1.0f);
    m_terrainMatl->set("hasRoughnessMap", 0.f);
    m_terrainMatl->set("hasMetallicRoughnessMap", 0.f);
    m_terrainMatl->set("uniformRoughness", 0.2f);
    m_terrainMatl->set("uniformMetallic", 0.1f);
    // debug view
    m_terrainMatl->set("debugG", 0.f);
    m_terrainMatl->set("debugF", 0.f);
    m_terrainMatl->set("debugD", 0.f);
    m_terrainMatl->set("disneyReparam", 1.f);
    terrainEntity->m_meshInstance->setMaterial(0, m_terrainMatl);

    m_scenes.push_back(helmetScene);

    m_helmetMatl = Cyan::createMaterial(m_pbrShader)->createInstance();
    // TODO: create a .cyanmatl file for definine materials?
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
    m_helmetMatl->set("hasRoughnessMap", 0.f);
    m_helmetMatl->set("hasMetallicRoughnessMap", 1.f);
    // debug view
    m_helmetMatl->set("debugG", 0.f);
    m_helmetMatl->set("debugF", 0.f);
    m_helmetMatl->set("debugD", 0.f);
    m_helmetMatl->set("disneyReparam", 1.f);
    SceneManager::getEntity(m_scenes[0], "DamagedHelmet")->m_meshInstance->setMaterial(0, m_helmetMatl);

    // add lights into the scene
    SceneManager::createPointLight(helmetScene, glm::vec3(0.9, 0.95f, 0.76f), glm::vec3(0.0f, 0.0f, 1.5f), 1.f);
    SceneManager::createPointLight(helmetScene, glm::vec3(0.6, 0.65f, 2.86f), glm::vec3(0.0f, 0.8f, -2.4f), 1.f);
    SceneManager::createDirectionalLight(helmetScene, glm::vec3(1.0), glm::normalize(glm::vec3(-1.0f, -0.5f, 5.f)), 1.f);
    SceneManager::createDirectionalLight(helmetScene, glm::vec3(1.0), glm::normalize(glm::vec3(-0.5f, -5.5f, -3.f)), 1.f);
    SceneManager::createDirectionalLight(helmetScene, glm::vec3(1.0), glm::normalize(glm::vec3(0.5f, -5.5f, -3.f)), 1.f);

    // manage entities
    SceneNode* helmetNode = helmetScene->m_root->find("DamagedHelmet");
    Transform centerTransform;
    centerTransform.m_translate = helmetNode->m_entity->m_instanceTransform.m_translate;
    Entity* pivotEntity = SceneManager::createEntity(helmetScene, "PointLightCenter", nullptr, centerTransform, true);
    SceneManager::createSceneNode(helmetScene, nullptr, pivotEntity);
    m_centerNode = helmetScene->m_root->find("PointLightCenter");
    SceneNode* pointLightNode = helmetScene->m_root->removeChild("PointLight0");
    m_centerNode->addChild(pointLightNode);
}

void PbrApp::initSpheresScene()
{
    const u32 kNumRows = 4u;
    const u32 kNumCols = 6u;

    Scene* sphereScene = Cyan::createScene("spheres_scene", "../../scene/default_scene/scene_spheres.json");
    sphereScene->mainCamera.projection = glm::perspective(glm::radians(sphereScene->mainCamera.fov), (float)(gEngine->getWindow().width) / gEngine->getWindow().height, sphereScene->mainCamera.n, sphereScene->mainCamera.f);

    Entity* envmapEntity = SceneManager::createEntity(sphereScene, "Envmap", "cubemapMesh", Transform(), false);
    envmapEntity->m_meshInstance->setMaterial(0, m_envmapMatl);

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
            Entity* entity = SceneManager::createEntity(sphereScene, nullptr, "sphere_mesh", transform, true);

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
    Cyan::Toolkit::createLightProbe("doge2", "../../asset/cubemaps/doge2.hdr",         true);
    Cyan::Toolkit::createLightProbe("studio", "../../asset/cubemaps/studio_01_4k.hdr",  true);
    Cyan::Toolkit:: createLightProbe("fire-sky", "../../asset/cubemaps/the_sky_is_on_fire_4k.hdr",  true);
    m_currentProbeIndex = 3u;
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

    // visualizer
    m_bufferVis = {};
    m_bufferVis.init(m_helmetMatl->m_uniformBuffer, glm::vec2(-0.6f, 0.8f), 0.8f, 0.05f);
    // clear color
    Cyan::getCurrentGfxCtx()->setClearColor(glm::vec4(0.0f, 0.0f, 0.0f, 1.f));

    // font
    ImGuiIO& io = ImGui::GetIO();
    m_font = io.Fonts->AddFontFromFileTTF("C:\\dev\\cyanRenderEngine\\lib\\imgui\\misc\\fonts\\Roboto-Medium.ttf", 16.f);
}

void PbrApp::beginFrame()
{
    Cyan::getCurrentGfxCtx()->clear();
    Cyan::getCurrentGfxCtx()->setViewport(0, 0, gEngine->getWindow().width, gEngine->getWindow().height);
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

void PbrApp::update()
{
    gEngine->processInput();
    // helmet scene
    Scene* scene = m_scenes[0];
    float pointLightsRotSpeed = .5f; // in degree
    float rotationSpeed = glm::radians(.5f);
    glm::quat qRot = glm::quat(cos(rotationSpeed * .5f), sin(rotationSpeed * .5f) * glm::normalize(glm::vec3(0.f, 1.f, 1.f)));
    m_centerNode->m_entity->m_instanceTransform.m_qRot *= qRot;
    m_centerNode->update();
    scene->pLights[0].position = scene->pLights[0].baseLight.m_entity->m_worldTransformMatrix[3]; 

    for (u32 i = 1; i < scene->pLights.size(); ++i)
    {
        glm::mat4 rotation = rotateAroundPoint(glm::vec3(0.f, 0.f, -1.5f), glm::vec3(0.f, 1.f, 0.f), pointLightsRotSpeed);
        pointLightsRotSpeed *= -1.f;
        scene->pLights[i].baseLight.m_entity->m_worldTransformMatrix = rotation * scene->pLights[i].baseLight.m_entity->m_worldTransformMatrix;
        scene->pLights[i].position = scene->pLights[i].baseLight.m_entity->m_worldTransformMatrix[3]; 
    }
}

void PbrApp::drawStatsWindow()
{
    m_ui.beginWindow("Frame stats");
    ImGui::PushFont(m_font);
    {
        ImGui::Text("Frame time:                   %.2f ms", m_lastFrameDurationInMs);
        ImGui::Text("Number of draw calls:         %d", 100u);
        ImGui::Text("Number of entities:           %d", m_scenes[m_currentScene]->entities.size());
        ImGui::Checkbox("Super Sampling 4x", &gEngine->getRenderer()->m_bSuperSampleAA);
    }
    ImGui::PopFont();
    m_ui.endWindow();
}

void drawSceneGraphUI(SceneNode* node) 
{
    ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow 
                                | ImGuiTreeNodeFlags_OpenOnDoubleClick 
                                | ImGuiTreeNodeFlags_SpanAvailWidth 
                                | ImGuiTreeNodeFlags_DefaultOpen;
    char* label = node->m_entity->m_name;
    if (node->m_child.size() > 0)
    {
        if (ImGui::TreeNodeEx(label, baseFlags))
        {
            for (auto* child : node->m_child)
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

void PbrApp::drawEntityWindow()
{
    m_ui.beginWindow("Entities");
    ImGui::PushFont(m_font);
    {
        Scene* scene = m_scenes[m_currentScene];
        std::queue<SceneNode*> nodesQueue;
        std::vector<SceneNode*> nodes;
        nodesQueue.push(scene->m_root);
        u32 depth = 0u;
        while (!nodesQueue.empty())
        {
            SceneNode* node = nodesQueue.front();
            nodesQueue.pop();
            nodes.push_back(node);
            for (auto* child : node->m_child)
            {
                nodesQueue.push(child);
            }
        }
        drawSceneGraphUI(scene->m_root);
    }
    ImGui::PopFont();
    m_ui.endWindow();
}

void PbrApp::drawLightingWindow()
{
    m_ui.beginWindow("Lighting");
    ImGui::PushFont(m_font);
    {
        // experimental
        if (m_ui.header("Experiemental"))
        {
            ImGui::Text("Wrap");
            ImGui::SameLine();
            ImGui::SliderFloat("##Wrap", &m_wrap, 0.f, 1.f, "%.2f");
        }
        // directional Lights
        if (m_ui.header("Directional Lights"))
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
                    ImGui::Text("Color:");
                    ImGui::ColorPicker3("##Color", &m_scenes[m_currentScene]->dLights[i].baseLight.color.r);
                    ImGui::TreePop();
                }
            }
        }
        // point lights
        if (m_ui.header("Point Lights"))
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
        }
    }
    ImGui::PopFont();
    m_ui.endWindow();
}

void PbrApp::drawSceneWindow()
{
    m_ui.beginWindow("Scene");
    ImGui::PushFont(m_font);
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
    ImGui::PopFont();
    m_ui.endWindow();
}

void PbrApp::render()
{
    // frame timer
    Cyan::Toolkit::ScopedTimer frameTimer("render()");

    Cyan::Renderer* renderer = gEngine->getRenderer();

    // stats window
    drawStatsWindow();
    // scene window
    drawSceneWindow();
    // lighting window
    drawLightingWindow();
    // entities window
    drawEntityWindow();

    m_helmetMatl->set("directDiffuseSlider", m_directDiffuseSlider);
    m_helmetMatl->set("directSpecularSlider", m_directSpecularSlider);
    m_helmetMatl->set("indirectDiffuseSlider", m_indirectDiffuseSlider);
    m_helmetMatl->set("indirectSpecularSlider", m_indirectSpecularSlider);
    m_helmetMatl->set("wrap", m_wrap);

    m_terrainMatl->set("directDiffuseSlider", m_directDiffuseSlider);
    m_terrainMatl->set("directSpecularSlider", m_directSpecularSlider);
    m_terrainMatl->set("indirectDiffuseSlider", m_indirectDiffuseSlider);
    m_terrainMatl->set("indirectSpecularSlider", m_indirectSpecularSlider);
    m_terrainMatl->set("wrap", m_wrap);

    // m_droneMatl->set("directDiffuseSlider", m_directDiffuseSlider);
    // m_droneMatl->set("directSpecularSlider", m_directSpecularSlider);
    // m_droneMatl->set("indirectDiffuseSlider", m_indirectDiffuseSlider);
    // m_droneMatl->set("indirectSpecularSlider", m_indirectSpecularSlider);
    // m_droneMatl->set("wrap", m_wrap);

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