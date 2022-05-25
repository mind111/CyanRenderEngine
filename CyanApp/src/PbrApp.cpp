#include <iostream>
#include <queue>
#include <functional>
#include <stdlib.h>
#include <thread>

#include "stb_image.h"
#include "ArHosekSkyModel.h"

#include "PbrApp.h"
#include "CyanUI.h"
#include "CyanAPI.h"
#include "Light.h"
#include "Shader.h"
#include "Mesh.h"

#define REBAKE_LIGHTMAP 0
#define MOUSE_PICKING   1

// In radians per pixel 
static float kCameraOrbitSpeed = 0.005f;
static float kCameraRotateSpeed = 0.005f;

#if 0
namespace Demo
{
    void mouseScrollWheelCallback(double xOffset, double yOffset)
    {
        DemoApp* app = DemoApp::get();
        if (!app->mouseOverUI())
        {
            CameraCommand command = { };
            command.type = CameraCommand::Type::Zoom;
            command.mouseWheelChange = glm::dvec2(xOffset, yOffset);
            app->dispatchCameraCommand(command);
        }
    }

    void keyCallback(i32 key, i32 action)
    {

    }
}
#endif

#define SCENE_DEMO_00

DemoApp::DemoApp(u32 appWindowWidth, u32 appWindowHeight)
    : DefaultApp(appWindowWidth, appWindowHeight), bOrbit(false)
{
    activeDebugViewTexture = nullptr;
    m_currentDebugView = -1;
}

bool DemoApp::mouseOverUI()
{
    return (m_mouseCursorX < 400.f && m_mouseCursorX > 0.f && m_mouseCursorY < 720.f && m_mouseCursorY > 0.f);
}

void DemoApp::setupScene()
{
    Cyan::Toolkit::GpuTimer timer("initDemoScene00()", true);

    auto sceneManager = SceneManager::get();
    m_scene = sceneManager->importScene("demo_scene_00", "C:\\dev\\cyanRenderEngine\\scene\\demo_scene_00.json");

    auto graphicsSystem = gEngine->getGraphicsSystem();
    glm::uvec2 viewportSize = graphicsSystem->getAppWindowDimension();
    float aspectRatio = (float)viewportSize.x / viewportSize.y;
    Camera& camera = m_scene->camera;
    camera.aspectRatio = aspectRatio;
    camera.projection = glm::perspective(glm::radians(camera.fov), aspectRatio, camera.n, camera.f);

    auto textureManager = Cyan::TextureManager::get();
    Scene* demoScene00 = m_scene.get();

    using PBR = Cyan::PBRMatl;
    auto assetManager = Cyan::AssetManager::get();
    PBR* defaultMatl = assetManager->createMaterial<PBR>("DefaultMatl");

    // helmet
    {
        auto helmet = sceneManager->getEntity(demoScene00, "DamagedHelmet");
        auto helmetMesh = helmet->getSceneNode("helmet_mesh")->getAttachedMesh()->parent;
#if 0
        helmetMesh->m_bvh = new Cyan::MeshBVH(helmetMesh);
        helmetMesh->m_bvh->build();
#endif
    }
    // bunnies
    {
        Entity* bunny0 = sceneManager->getEntity(demoScene00, "Bunny0");
        Entity* bunny1 = sceneManager->getEntity(demoScene00, "Bunny1");

        auto bunnyMatl = assetManager->createMaterial<PBR>("BunnyMatl");
        bunnyMatl->parameter.kAlbedo = glm::vec3(0.855, 0.647, 0.125f);
        bunnyMatl->parameter.kRoughness = .1f;
        bunnyMatl->parameter.kMetallic = 1.0f;

        bunny0->setMaterial("bunny_mesh", -1, bunnyMatl);
        bunny1->setMaterial("bunny_mesh", -1, bunnyMatl);
    }
    // man
    {
        Entity* man = sceneManager->getEntity(demoScene00, "Man");
        auto manMatl = assetManager->createMaterial<PBR>("ManMatl");
        manMatl->parameter.kAlbedo = glm::vec3(0.855, 0.855, 0.855);
        manMatl->parameter.kRoughness = .3f;
        manMatl->parameter.kMetallic = .3f;

        man->setMaterial("man_mesh", -1, manMatl);
    }
    // lighting
    // todo: refactor lights, decouple them from Entity
    {
        // sun light
        glm::vec3 sunDir = glm::normalize(glm::vec3(1.0f, 0.5f, 1.8f));
        sceneManager->createDirectionalLight(demoScene00, glm::vec3(1.0f, 0.9, 0.7f), sunDir, 3.6f);

        // skybox
#if 0
        demoScene00->m_skybox = new Cyan::Skybox(nullptr, glm::uvec2(1024u), Cyan::SkyboxConfig::kUseProcedural);
        demoScene00->m_skybox->build();

        // light probes
        demoScene00->m_irradianceProbe = sceneManager->createIrradianceProbe(demoScene00, glm::vec3(0.f, 2.f, 0.f), glm::uvec2(512u, 512u), glm::uvec2(64u, 64u));
        demoScene00->m_reflectionProbe = sceneManager->createReflectionProbe(demoScene00, glm::vec3(0.f, 3.f, 0.f), glm::uvec2(2048u, 2048u));
#endif
    }

    // grid of shader ball on the table
    {
        glm::vec3 gridLowerLeft(-2.1581, 1.1629, 2.5421);
        glm::vec3 defaultAlbedo(1.f);
        glm::vec3 silver(.753f, .753f, .753f);
        glm::vec3 gold(1.f, 0.843f, 0.f);
        glm::vec3 chrome(1.f);
        glm::vec3 plastic(0.061f, 0.757f, 0.800f);

        PBR* shaderBallMatls[2][4] = { };
        shaderBallMatls[0][0] = assetManager->createMaterial<PBR>("ShaderBallGold");
        shaderBallMatls[0][0]->parameter.kRoughness = 0.02f;
        shaderBallMatls[0][0]->parameter.kMetallic = 0.05f;
/*
        matlParams[0][3].kRoughness = .02f;
        matlParams[0][3].kMetallic = 0.05f;
        matlParams[0][3].kSpecular = 1.f;
        matlParams[1][0].flatBaseColor = glm::vec4(gold, 1.f);
        matlParams[1][0].roughness = textureManager->getTexture("imperfection_grunge");
        matlParams[1][0].kMetallic = 0.95f;
        matlParams[1][1].baseColor = textureManager->getTexture("green_marble_albedo");
        matlParams[1][1].roughness = textureManager->getTexture("green_marble_roughness");
        matlParams[1][1].kMetallic = 0.01f;
        matlParams[1][2].baseColor = textureManager->getTexture("white_marble_albedo");
        matlParams[1][2].roughness = textureManager->getTexture("white_marble_roughness");
        matlParams[1][2].kMetallic = 0.01f;
        matlParams[1][3].baseColor = textureManager->getTexture("brick_albedo");
        matlParams[1][3].roughness = textureManager->getTexture("brick_roughness");
        matlParams[1][3].normal = textureManager->getTexture("brick_nm");
        matlParams[1][3].kMetallic = 0.01f;
        matlParams[1][3].kSpecular = 0.01f;

        Cyan::PbrMaterialParam matlParams[2][4] = { };
        matlParams[0][0].flatBaseColor = glm::vec4(gold, 1.f);
        matlParams[0][0].kRoughness = .1f;
        matlParams[0][0].kMetallic = 0.999f;
        matlParams[0][1].flatBaseColor = glm::vec4(silver, 1.f);
        matlParams[0][1].kRoughness = .02f;
        matlParams[0][1].kMetallic = 1.0;
        matlParams[0][2].flatBaseColor = glm::vec4(chrome, 1.f);
        matlParams[0][2].kRoughness = .3f;
        matlParams[0][2].kMetallic = 0.999f;
        matlParams[0][3].flatBaseColor = glm::vec4(plastic, 1.f);
        matlParams[0][3].kRoughness = .02f;
        matlParams[0][3].kMetallic = 0.05f;
        matlParams[0][3].kSpecular = 1.f;
        matlParams[1][0].flatBaseColor = glm::vec4(gold, 1.f);
        matlParams[1][0].roughness = textureManager->getTexture("imperfection_grunge");
        matlParams[1][0].kMetallic = 0.95f;
        matlParams[1][1].baseColor = textureManager->getTexture("green_marble_albedo");
        matlParams[1][1].roughness = textureManager->getTexture("green_marble_roughness");
        matlParams[1][1].kMetallic = 0.01f;
        matlParams[1][2].baseColor = textureManager->getTexture("white_marble_albedo");
        matlParams[1][2].roughness = textureManager->getTexture("white_marble_roughness");
        matlParams[1][2].kMetallic = 0.01f;
        matlParams[1][3].baseColor = textureManager->getTexture("brick_albedo");
        matlParams[1][3].roughness = textureManager->getTexture("brick_roughness");
        matlParams[1][3].normal = textureManager->getTexture("brick_nm");
        matlParams[1][3].kMetallic = 0.01f;
        matlParams[1][3].kSpecular = 0.01f;
*/
        using namespace Cyan;
        for (u32 j = 0; j < 2; ++j)
        {
            for (u32 i = 0; i < 4; ++i)
            {
                char entityName[32];
                char meshNodeName[32];
                sprintf_s(entityName, "ShaderBall%u", j * 4 + i);
                sprintf_s(meshNodeName, "ShaderBall%u", j * 4 + i);
                auto shaderBall = sceneManager->createEntity(demoScene00, entityName, Transform{}, false);

                glm::vec3 posOffset = glm::vec3(1.3f * (f32)i, 0.f, -1.5f * (f32)j);
                Transform transform = { };
                transform.m_translate = gridLowerLeft + posOffset;
                transform.m_scale = glm::vec3(.003f);
                auto meshNode = sceneManager->createMeshNode(demoScene00, transform, assetManager->getAsset<Mesh>("shaderball_mesh"));
                shaderBall->attachSceneNode(meshNode);
                shaderBall->setMaterial(meshNodeName, -1, defaultMatl);
                shaderBall->setMaterial(meshNodeName, 0, defaultMatl);
                shaderBall->setMaterial(meshNodeName, 1, defaultMatl);
                shaderBall->setMaterial(meshNodeName, 5, defaultMatl);
            }
        }
    }
    // room
    {
        Entity* room = sceneManager->getEntity(demoScene00, "Room");
        room->setMaterial("room_mesh", -1, defaultMatl);
        room->setMaterial("plane_mesh", -1, defaultMatl);
#if 0
        Entity* room = sceneManager->getEntity(demoScene00, "Room");
        auto roomNode = room->getSceneNode("RoomMesh");
        auto planeNode = room->getSceneNode("PlaneMesh");
        Cyan::Mesh* roomMesh = roomNode->m_meshInstance->m_mesh;
        Cyan::Mesh* planeMesh = planeNode->m_meshInstance->m_mesh;
        roomMesh->m_bvh = new Cyan::MeshBVH(roomMesh);
        roomMesh->m_bvh->build();
#endif

#if REBAKE_LIGHTMAP
        // default matl param
        Cyan::PbrMaterialParam params = { };
        params.flatBaseColor = glm::vec4(1.f);
        params.kRoughness = 0.8f;
        params.kMetallic = 0.02f;
        params.hasBakedLighting = 1.f;
        for (u32 sm = 0; sm < roomMesh->numSubMeshes(); ++sm)
        {

            i32 matlIdx = roomMesh->m_subMeshes[sm]->m_materialIdx;
            if (matlIdx >= 0)
            {
                auto& objMatl = roomMesh->m_objMaterials[matlIdx];
                params.flatBaseColor = glm::vec4(objMatl.diffuse, 1.f);
            }
            auto matl = createStandardPbrMatlInstance(demoScene00, params, room->m_static);
            room->setMaterial("RoomMesh", sm, matl);
        }
        params.usePrototypeTexture = 1.f;
        auto planeMatl = createStandardPbrMatlInstance(demoScene00, params, room->m_static);
        room->setMaterial("PlaneMesh", -1, planeMatl);

        // bake lightmap
        auto lightMapManager = Cyan::LightMapManager::get();
        lightMapManager->bakeLightMap(m_scenes[Scenes::Demo_Scene_00], roomNode, true);
        lightMapManager->bakeLightMap(m_scenes[Scenes::Demo_Scene_00], planeNode, true);

        for (u32 sm = 0; sm < roomMesh->numSubMeshes(); ++sm)
        {
            auto matl = roomNode->m_meshInstance->m_matls[sm];
            matl->bindTexture("lightMap", roomNode->m_meshInstance->m_lightMap->m_texAltas);
        }
        for (u32 sm = 0; sm < planeMesh->numSubMeshes(); ++sm)
        {
            auto matl = planeNode->m_meshInstance->m_matls[sm];
            matl->bindTexture("lightMap", planeNode->m_meshInstance->m_lightMap->m_texAltas);
        }
#else
        // directly use baked lightmap
#if 0
        auto lightMapManager = Cyan::LightMapManager::get();
        lightMapManager->createLightMapFromTexture(roomNode, textureManager->getTexture("RoomMesh_lightmap"));
        lightMapManager->createLightMapFromTexture(planeNode, textureManager->getTexture("PlaneMesh_lightmap"));
        for (u32 sm = 0; sm < roomMesh->numSubMeshes(); ++sm)
        {
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.uniformRoughness", .8f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.uniformMetallic", .02f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMaterialProps.hasBakedLighting", 1.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.indirectDiffuseScale", 0.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.indirectSpecularScale", 0.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMaterialProps.hasBakedLighting", 1.f);
            roomNode->m_meshInstance->m_matls[sm]->set("uMatlData.flags", Cyan::StandardPbrMaterial::Flags::kUseLightMap);
            roomNode->m_meshInstance->m_matls[sm]->bindTexture("lightMap", roomNode->m_meshInstance->m_lightMap->m_texAltas);
        }
        Cyan::PbrMaterialParam params = { };
        params.flatBaseColor = glm::vec4(1.f);
        params.kRoughness = 0.8f;
        params.kMetallic = 0.0f;
        params.hasBakedLighting = 1.f;
        params.usePrototypeTexture = 1.f;
        params.lightMap = planeNode->m_meshInstance->m_lightMap->m_texAltas;

        auto planeMatl = assetManager->createMaterial<Cyan::LightMappedMaterial<PBR>>("RoomMaterial");

        // auto planeMatl = createStandardPbrMatlInstance(demoScene00, params, room->m_static);
        room->setMaterial("PlaneMesh", -1, planeMatl);
        sceneManager->updateSceneGraph(demoScene00);
#endif
#endif
    }
    timer.end();
}

void DemoApp::customInitialize()
{
    Cyan::Toolkit::GpuTimer timer("DemoApp::customInitialize()", true);
    {
        // setup user input control
        auto IOSystem = gEngine->getIOSystem();
        IOSystem->addIOEventListener<Cyan::MouseCursorEvent>([this, IOSystem](f64 xPos, f64 yPos) {
            glm::dvec2 mouseCursorChange = IOSystem->getMouseCursorChange();
            if (bOrbit)
            {
                orbitCamera(m_scene->camera, mouseCursorChange.x, mouseCursorChange.y);
            }
        });

        IOSystem->addIOEventListener<Cyan::MouseButtonEvent>([this](i32 button, i32 action) {
            switch(button)
            {
                case CYAN_MOUSE_BUTTON_RIGHT:
                {
                    if (action == CYAN_PRESS)
                    {
                        bOrbit = true;
                    }
                    else if (action == CYAN_RELEASE)
                    {
                        bOrbit = false;
                    }
                    break;
                }
                default:
                    break;
            }
        });

        IOSystem->addIOEventListener<Cyan::MouseWheelEvent>([this](f64 xOffset, f64 yOffset) {

        });

        IOSystem->addIOEventListener<Cyan::KeyEvent>([](i32 key, i32 action) {

        });

        // setup scene
        setupScene();
    }
    timer.end();
}

void DemoApp::precompute()
{
    // build lighting
#ifdef SCENE_DEMO_00 
    m_scene->m_irradianceProbe->build();
    m_scene->m_reflectionProbe->build();
#endif
#ifdef SCENE_SPONZA
    auto pathTracer = Cyan::PathTracer::get();
    pathTracer->setScene(m_scenes[Sponza_Scene]);
    pathTracer->run(m_scenes[Sponza_Scene]->getActiveCamera());
#endif
#if 0
#ifdef SCENE_DEMO_00
    auto pathTracer = Cyan::PathTracer::get();
    pathTracer->setScene(m_scenes[Demo_Scene_00]);
    pathTracer->run(m_scenes[Demo_Scene_00]->getActiveCamera());
    pathTracer->fillIrradianceCacheDebugData(m_scenes[Demo_Scene_00]->getActiveCamera());
    m_debugLines.resize(pathTracer->m_irradianceCache->m_numRecords);
    glm::vec4 color(0.f, 0.f, 1.f, 1.f);
    for (u32 i = 0; i < m_debugLines.size(); ++i)
    {
        auto& record = pathTracer->m_irradianceCache->m_records[i];
        auto& translationalGradient = pathTracer->m_debugData.translationalGradients[i];
        m_debugLines[i].init();
        m_debugLines[i].setVertices(record.position, record.position + translationalGradient);
        m_debugLines[i].setColor(color);
    }
#endif
#endif
}

void DemoApp::customUpdate()
{

}

void DemoApp::debugIrradianceCache()
{
    // auto pathTracer = Cyan::PathTracer::get();
    auto renderer = Cyan::Renderer::get();
    auto assetManager = Cyan::AssetManager::get();
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
    auto cubeMesh = assetManager->getAsset<Cyan::Mesh>("CubeMesh");
    auto circleMesh = assetManager->getAsset<Cyan::Mesh>("circle_mesh");
    auto debugShader = Cyan::ShaderManager::getShader("DebugShadingShader");
    Camera& camera = m_scene->camera;
    glm::mat4 vp = camera.projection * camera.view;
    glm::vec4 color0(1.f, 1.f, 1.f, 1.f);
    glm::vec4 color1(0.f, 1.f, 0.f, 1.f);
    glm::vec4 color2(0.f, 0.f, 1.f, 1.f);

    auto debugDrawCube = [&](const glm::vec3& pos, const glm::vec3& scale, glm::vec4& color) {
        ctx->setShader(debugShader);
        ctx->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
        Transform transform;
        transform.m_translate = pos;
        transform.m_scale = scale;
        glm::mat4 mvp = vp * transform.toMatrix();
        debugShader->setUniform("color", color);
        for (u32 sm = 0; sm < cubeMesh->numSubmeshes(); ++sm)
        {
            debugShader->setUniform("mvp", mvp);
            renderer->drawMesh(cubeMesh);
        }
    };

    auto debugDrawCircle = [&](const glm::vec3& pos, const glm::vec3& n, const glm::vec3& scale, glm::vec4& color) {
        ctx->setShader(debugShader);
        ctx->setPrimitiveType(Cyan::PrimitiveType::Line);
        glm::mat3 tangentFrame = Cyan::tangentToWorld(n);
        glm::mat4 rotation = { 
            glm::vec4(tangentFrame[0], 0.f),
            glm::vec4(tangentFrame[2], 0.f),
            glm::vec4(tangentFrame[1], 0.f),
            glm::vec4(0.f, 0.f, 0.f, 1.f)
        };
        glm::mat4 m(1.f);
        m = glm::translate(m, pos);
        m *= rotation;
        m = glm::scale(m, glm::vec3(scale));
        glm::mat4 mvp = vp * m;
        debugShader->setUniform("color", color);
        for (u32 sm = 0; sm < circleMesh->numSubmeshes(); ++sm)
        {
            debugShader->setUniform("mvp", mvp);
            renderer->drawMesh(circleMesh);
        }
    };

    glDisable(GL_CULL_FACE);
    {
#if 0
        // visualize irradiance records
        u32 start = pathTracer->m_irradianceCache->m_numRecords * .0f;
        u32 end = pathTracer->m_irradianceCache->m_numRecords * 1.f;
        for (u32 i = start; i < end; ++i)
        {
            auto& record = pathTracer->m_irradianceCache->m_records[i];
            debugDrawCircle(record.position, record.normal, glm::vec3(record.r), color0);
            debugDrawCube(record.position, glm::vec3(record.r * .02f), color1);
        }
#endif
#if 0
        // visualize translational gradients
        for (u32 i = 0; i < m_debugLines.size(); ++i)
        {
            m_debugLines[i].draw(vp);
        }
        // visualize octree
        for (u32 i = 0; i < pathTracer->m_debugData.octreeBoundingBoxes.size(); ++i)
        {
            pathTracer->m_debugData.octreeBoundingBoxes[i].draw(vp);
        }
#endif
    }
    glEnable(GL_CULL_FACE);
}

void DemoApp::customRender()
{
#if 0
    debugIrradianceCache();
    drawSceneViewport();
    drawDebugWindows();

    // frame timer
    Cyan::Toolkit::GpuTimer frameTimer("render()");
    {
        auto renderer = Cyan::Renderer::get();
        // scene & debug objects
        renderer->render(m_scene.get(), [this]() {
            debugIrradianceCache();
        });
        // ui
        renderer->renderUI([this]() {
            drawSceneViewport();
            drawDebugWindows();
        });
        // request flip
        renderer->flip();
    }
    frameTimer.end();
    m_lastFrameDurationInMs = frameTimer.m_durationInMs;
#endif
}

void DemoApp::customFinalize()
{

}

void DemoApp::zoomCamera(Camera& camera, double dx, double dy)
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

void DemoApp::orbitCamera(Camera& camera, double deltaX, double deltaY)
{
    // orbit camera
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

void DemoApp::rotateCamera(double deltaX, double deltaY)
{

}
