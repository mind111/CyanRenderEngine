
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "CyanEngine.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "CyanApp.h"
#include "World.h"
#include "CameraComponent.h"
#include "CameraEntity.h"
#include "LightEntities.h"
#include "LightComponents.h"
#include "SkyboxEntity.h"

namespace Cyan
{
    class Editor : public DefaultApp
    {
    public:
        Editor()
            : DefaultApp(1920, 1080)
        {
        }

        virtual void customInitialize() override
        {
            static const char* shaderBalls = ASSET_PATH "mesh/shader_balls.glb";
            static const char* sponza = ASSET_PATH "mesh/sponza-gltf-pbr/sponza.glb";
            static const char* sunTemple = ASSET_PATH "mesh/sun_temple/simplified_sun_temple.glb";
            static const char* ueArchviz = ASSET_PATH "mesh/archviz.glb";
            static const char* diorama = ASSET_PATH "mesh/sd_macross_diorama.glb";
            static const char* picapica = ASSET_PATH "mesh/pica_pica_scene.glb";

            m_world->import(shaderBalls);

            Transform local;
            local.translation = glm::vec3(0.f, 3.f, 8.f);
            m_editorCameraEntity = m_world->createCameraEntity("EditorCamera", local, m_appWindowDimension);
            m_editorCameraEntity->addComponent(std::make_shared<CameraControllerComponent>("CameraController", m_editorCameraEntity->getCameraComponent()));
            
            // add a debug camera for viewing the
            Transform t;
            t.translation = glm::vec3(5.f, 3.f, 8.f);
            f32 halfAngle = glm::radians(90.f * .5f);
            t.rotation = glm::quat(glm::cos(halfAngle), glm::vec3(0.f, 1.f, 0.f) * glm::sin(halfAngle));
            m_shadowDebugCameraEntity = m_world->createCameraEntity("ShadowDebugCamera", t, m_appWindowDimension);
            m_shadowDebugCameraEntity->getCameraComponent()->turnOff();
            m_shadowDebugCameraEntity->addComponent(std::make_shared<CameraControllerComponent>("CameraController", m_shadowDebugCameraEntity->getCameraComponent()));

            // register a callback for toggling between camera view
            m_engine->getInputSystem()->registerKeyEventListener('T', [this](const KeyEvent& event) {
                    static i32 activeCamerIndex = 0;
                    bool bSwitchCamera = false;
                    switch (event.action)
                    {
                    case KEY_PRESSED: 
                        bSwitchCamera = true;
                        break;
                    case KEY_REPEAT:
                    case KEY_RELEASE:
                    default: break;
                    }
                    CameraEntity* cameraEntities[2] = { m_editorCameraEntity, m_shadowDebugCameraEntity };
                    if (bSwitchCamera)
                    {
                        assert(m_editorCameraEntity != nullptr);
                        assert(m_shadowDebugCameraEntity != nullptr);

                        i32 currentActiveCameraIndex = activeCamerIndex;
                        cameraEntities[currentActiveCameraIndex]->getCameraComponent()->turnOff();
                        {
                            std::vector<CameraControllerComponent*> cameraControllers;
                            cameraEntities[currentActiveCameraIndex]->getComponent<CameraControllerComponent>(cameraControllers);
                            for (auto controller : cameraControllers)
                            {
                                controller->turnOff();
                            }
                        }

                        i32 nextActiveCameraIndex = (currentActiveCameraIndex + 1) % 2;
                        cameraEntities[nextActiveCameraIndex]->getCameraComponent()->turnOn();
                        {
                            std::vector<CameraControllerComponent*> cameraControllers;
                            cameraEntities[nextActiveCameraIndex]->getComponent<CameraControllerComponent>(cameraControllers);
                            for (auto controller : cameraControllers)
                            {
                                controller->turnOn();
                            }
                        }

                        activeCamerIndex = nextActiveCameraIndex;
                        m_mainCameraIndex = activeCamerIndex;
                    }
                }
            );

            m_engine->getInputSystem()->registerKeyEventListener('Y', [this](const KeyEvent& event) {
                    switch (event.action)
                    {
                    case KEY_PRESSED: 
                        m_bDebugShadow = !m_bDebugShadow;
                        break;
                    case KEY_REPEAT:
                    case KEY_RELEASE:
                    default: break;
                    }
                }
            );

            auto directionalLightEntity = m_world->createDirectionalLightEntity("DirectionalLight", Transform());
            auto directionalLightComponent = directionalLightEntity->getDirectionalLightComponent();
            directionalLightComponent->setIntensity(30.f);

            auto skyLightEntity = m_world->createSkyLightEntity("SkyLightEntity", Transform());
            auto skyLightComponent = skyLightEntity->getSkyLightComponent();
            // todo: this is a bit awkward, an image should be equivalent to a texture, need to 
            // decouple Sampler from Texture2D, also want to implement a virtual file system 
            auto HDRIImage = AssetImporter::importImage("HDRI", ASSET_PATH "cubemaps/neutral_sky.hdr");
            auto HDRITexture = AssetManager::createTexture2D("HDRITexture", HDRIImage, Sampler2D());
            skyLightComponent->setHDRI(HDRITexture);
            skyLightComponent->capture();

            auto skyboxEntity = m_world->createSkyboxEntity("SkyboxEntity", Transform());

            auto testMaterial = AssetManager::createMaterial("M_Test", MATERIAL_SOURCE_PATH "M_Test_p.glsl", [](MaterialInstance* instance) {
                    instance->setFloat("mp_Roughness", .5f);
                    instance->setVec3("mp_Albedo", glm::vec3(1.f, 0.f, 0.f));
                });

            // overwrite the default rendering lambda
            m_renderOneFrame = [this](GfxTexture2D* renderingOutput) {
                auto renderer = Renderer::get();
                // scene rendering
                if (m_world != nullptr) 
                {
                    auto scene = m_world->getScene();
                    auto sceneCamera = scene->m_sceneCameras[m_mainCameraIndex];
                    auto editorCamera = m_editorCameraEntity->getCameraComponent()->getSceneCamera();
                    editorCamera->setRenderMode((SceneCamera::RenderMode)m_renderMode);
                    scene->render();
                    renderer->renderToScreen(sceneCamera->getRender());
                }
                // UI rendering
                renderEditorUI();
                renderer->renderUI();
            };
        }

        void renderEditorUI()
        {
            auto renderer = Renderer::get();
            renderer->addUIRenderCommand([this]() {
                ImGui::Begin("Scene Inspector", nullptr);
                {
                    class SceneInspectorPanel
                    {
                    public:
                        SceneInspectorPanel(World* world)
                            : m_world(world)
                        {

                        }

                        void renderEntity(Entity* entity)
                        {
                            if (entity)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;
                                if (selectedEntity && (selectedEntity->getName() == entity->getName()))
                                {
                                    flags |= ImGuiTreeNodeFlags_Selected;
                                }

                                if (entity->numChildren() > 0)
                                {
                                    bool open = ImGui::TreeNodeEx(entity->getName().c_str(), flags);
                                    if (ImGui::IsItemClicked())
                                    {
                                        selectedEntity = entity;
                                    }
                                    ImGui::TableNextColumn();
                                    if (open)
                                    {
                                        entity->forEachChild([this](Entity* child) {
                                                renderEntity(child);
                                            });
                                        ImGui::TreePop();
                                    }
                                }
                                else
                                {
                                    flags |= ImGuiTreeNodeFlags_Leaf;
                                    bool open = ImGui::TreeNodeEx(entity->getName().c_str(), flags);
                                    if (ImGui::IsItemClicked())
                                    {
                                        selectedEntity = entity;
                                    }
                                    ImGui::TableNextColumn();
                                    if (open)
                                    {
                                        ImGui::TreePop();
                                    }
                                }
                            }
                        }

                        void render()
                        {
                            static ImGuiTableFlags flags = ImGuiTableFlags_RowBg
                                | ImGuiTableFlags_NoBordersInBody;
                            if (m_world)
                            {
                                if (ImGui::BeginTable("##SceneHierarchy", 2, flags, ImVec2(ImGui::GetWindowContentRegionWidth(), 100.f)))
                                {
                                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                                    ImGui::TableSetupColumn("Type");
                                    ImGui::TableHeadersRow();

                                    renderEntity(m_world->m_rootEntity);

                                    ImGui::EndTable();
                                }

                            }
                        }

                        Entity* selectedEntity = nullptr;
                        World* m_world = nullptr;
                    };

                    static SceneInspectorPanel sceneInspector(m_world.get());
                    sceneInspector.render();
                }
                ImGui::End();

                ImGui::Begin("Renderer Settings", nullptr, ImGuiWindowFlags_MenuBar);
                {
                    auto renderer = Renderer::get();

                    // todo: this pointer is unsafe as it points to an element in a vector which will
                    // likely be changed when the vector is resized.
                    static Cyan::Renderer::VisualizationDesc* selectedVis = nullptr;
                    // rendering main menu
                    if (ImGui::BeginMenuBar()) 
                    {
                        if (ImGui::BeginMenu("Options"))
                        {
                            ImGui::MenuItem("dummy_0", nullptr, false);
                            ImGui::MenuItem("dummy_1", nullptr, false);
                            ImGui::MenuItem("dummy_2", nullptr, false);
                            ImGui::EndMenu();
                        }
                        if (ImGui::BeginMenu("Visualization"))
                        {
                            for (auto& categoryEntry : renderer->visualizationMap) 
                            {
                                const std::string& category = categoryEntry.first;
                                auto& visualizations = categoryEntry.second;
                                if (ImGui::BeginMenu(category.c_str())) {
                                    for (auto& vis : visualizations) {
                                        bool selected = false;
                                        if (selectedVis && selectedVis->texture) 
                                        {
                                            selected = (selectedVis->m_name == vis.m_name);
                                        }
                                        if (vis.texture) {
                                            if (ImGui::MenuItem(vis.m_name.c_str(), nullptr, selected)) {
                                                if (selected) {
                                                    if (selectedVis->bSwitch) 
                                                    {
                                                        *(selectedVis->bSwitch) = false;
                                                    }
                                                    selectedVis = nullptr;
                                                }
                                                else 
                                                {
                                                    selectedVis = &vis;
                                                    if (selectedVis->bSwitch) 
                                                    {
                                                        *(selectedVis->bSwitch) = true;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    ImGui::EndMenu();
                                }
                            }
                            ImGui::EndMenu();
                        }
                        ImGui::EndMenuBar();
                    }
                    const char* renderModeNames[(u32)SceneCamera::RenderMode::kCount] = {
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSceneColor),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kResolvedSceneColor),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSceneAlbedo),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSceneDepth),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSceneNormal),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSceneDirectLighting),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSceneIndirectLighting),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSceneLightingOnly),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSSGIAO),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kSSGIDiffuse),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kDebug),
                        SceneCamera::renderModeName(SceneCamera::RenderMode::kWireframe)
                    };
                    ImGui::Combo("RenderMode", &m_renderMode, renderModeNames, (i32)SceneCamera::RenderMode::kCount);
                    if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::TextColored(ImVec4(0.f, 1.f, 1.f, 1.f), "Frame Time: %.2f ms", m_engine->getFrameElapsedTimeInMs());
                    }
                    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Text("Direct Lighting");
                        ImGui::Separator();
                        ImGui::Text("Indirect Lighting");
                        ImGui::Checkbox("Ambient Occlusion", &renderer->m_settings.bSSAOEnabled);
                        ImGui::Checkbox("Bent Normal", &renderer->m_settings.bBentNormalEnabled);
                        ImGui::Checkbox("Indirect Irradiance", &renderer->m_settings.bIndirectIrradianceEnabled);
                    }
#if 0
                    if (ImGui::CollapsingHeader("SSGI", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        auto SSGI = SSGI::get();
                        ImGui::Text("Ambient Occlusion");
                        ImGui::Checkbox("Bilateral Filtering", &SSGI->bBilateralFiltering);
                        ImGui::Separator();
                        ImGui::Text("Indirect Irradiance");
                        ImGui::Text("Normal Error Tolerance:");
                        ImGui::SliderFloat("##Normal Error Tolerance: ", &SSGI->indirectIrradianceNormalErrTolerance, -1.f, 0.f);
                        float v[2] = { SSGI->debugPixelCoord.x, SSGI->debugPixelCoord.y };
                        ImGui::SliderFloat2("##Debug Pixel Coord: ", v, 0.f, 1.f);
                        SSGI->debugPixelCoord.x = v[0];
                        SSGI->debugPixelCoord.y = v[1];
                        ImGui::Checkbox("Use ReSTIR:", &SSGI->bUseReSTIR);
                        ImGui::SliderInt("Num of Spatial Reuse Samples:", &SSGI->numSpatialReuseSamples, 1, 64);
                        ImGui::SliderFloat("ReSTIR Spatial Reuse Kernel Radius:", &SSGI->ReSTIRSpatialReuseKernalRadius, 0.f, 1.f);
                        ImGui::Separator();
                        ImGui::Text("Reflection");
                        ImGui::Separator();

                        ImGui::Checkbox("Debug SSRT", &renderer->bDebugSSRT);
                        if (renderer->bDebugSSRT)
                        {
                            static f32 debugCoord[2] = { 0.5f , 0.5f };
                            ImGui::Text("Debug Coord:"); 
                            ImGui::SameLine();
                            ImGui::SliderFloat2("##Debug Coord: ", debugCoord, 0.f, 1.f, "%.3f");
                            renderer->debugCoord = glm::vec2(debugCoord[0], debugCoord[1]);
                            ImGui::Checkbox("Fix Debug Ray", &renderer->bFixDebugRay);
                        }
                        ImGui::Text("Max Num Trace Steps Per Ray:"); ImGui::SameLine();
                        ImGui::SliderInt("##Max Num Trace Steps Per Ray:", &SSGI->numIterations, 1, SSGI->kMaxNumIterations);

                        ImGui::Text("Spatial Reuse Kernel Radius:"); 
                        ImGui::SameLine();
                        ImGui::SliderFloat("##Spatial Reuse Kernel Radius:", &SSGI->reuseKernelRadius, 0.f, 1.f);

                        ImGui::Text("Spatial Reuse Sample Count:"); 
                        ImGui::SameLine();
                        ImGui::SliderInt("##Spatial Reuse Sample Count:", &SSGI->numReuseSamples, 1, SSGI->kMaxNumReuseSamples);
                    }
#endif
                    if (ImGui::CollapsingHeader("Post Processing"))
                    {
                        ImGui::TextUnformatted("Color Temperature"); ImGui::SameLine();
                        // todo: render a albedo temperature slider image and blit to ImGui
                        // ImGui::Image((void*)1, ImVec2(ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeight())); ImGui::SameLine();
                        ImGui::SliderFloat("##Color Temperature", &renderer->m_settings.colorTempreture, 1000.f, 10000.f, "%.2f");

                        ImGui::TextUnformatted("Bloom"); ImGui::SameLine();
                        ImGui::Checkbox("##Bloom", &renderer->m_settings.enableBloom);

                        ImGui::TextUnformatted("Exposure"); ImGui::SameLine();
                        ImGui::SliderFloat("##Exposure", &renderer->m_settings.exposure, 0.f, 100.f);

                        ImGui::TextUnformatted("Tone Mapping"); ImGui::SameLine();
                        ImGui::Checkbox("##Tone Mapping", &renderer->m_settings.enableTonemapping);
                        if (renderer->m_settings.enableTonemapping) {
                            const char* tonemapOperatorNames[(u32)Cyan::Renderer::TonemapOperator::kCount] = { "Reinhard", "ACES", "Smoothstep" };
                            i32 currentOperator = (i32)renderer->m_settings.tonemapOperator;
                            ImGui::Combo("Tonemap Operator", &currentOperator, tonemapOperatorNames, (i32)Cyan::Renderer::TonemapOperator::kCount);
                            renderer->m_settings.tonemapOperator = (u32)currentOperator;
                            switch ((Cyan::Renderer::TonemapOperator)renderer->m_settings.tonemapOperator) {
                            case Cyan::Renderer::TonemapOperator::kReinhard:
                                ImGui::SliderFloat("White Point Luminance", &renderer->m_settings.whitePointLuminance, 1.f, 200.f, "%.2f");
                                break;
                            case Cyan::Renderer::TonemapOperator::kACES:
                            case Cyan::Renderer::TonemapOperator::kSmoothstep:
                                ImGui::SliderFloat("White Point", &renderer->m_settings.smoothstepWhitePoint, 0.f, 100.f, "%.2f");
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    renderer->setVisualization(selectedVis);
                    if (selectedVis)
                    {
                        if (ImGui::CollapsingHeader("Texture Visualization", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            auto texture = selectedVis->texture;
                            if (texture) 
                            {
                                ImGui::Text("%s", selectedVis->m_name);
                                ImGui::SliderInt("Active Mip:", &selectedVis->activeMip, 0, texture->numMips - 1);
                            }
                        }
                    }
                    if (ImGui::CollapsingHeader("Rendering Resource Usage"))
                    {
                        ImGui::Text("Num RenderTexture2D allocated: %u", RenderTexture2D::getNumAllocatedGfxTexture2D());
                    }
                }
                ImGui::End();
            });
        }
private:
        CameraEntity* m_editorCameraEntity = nullptr;
        CameraEntity* m_shadowDebugCameraEntity = nullptr;
        i32 m_mainCameraIndex = 0;
        i32 m_renderMode = 1;
        bool m_bDebugShadow = false;
    };
}

int main()
{
    std::unique_ptr<Cyan::Editor> editor = std::make_unique<Cyan::Editor>();
    editor->run();
    return 0;
}