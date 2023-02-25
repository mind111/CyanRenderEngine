
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "CyanEngine.h"
#include "AssetImporter.h"
#include "AssetManager.h"
#include "CyanApp.h"

namespace Cyan
{
    class Editor : public DefaultApp
    {
    public:
        Editor()
            : DefaultApp(1024, 1024)
        {
        }

        virtual void customInitialize() override
        {
            static const char* shaderBalls = ASSET_PATH "mesh/shader_balls.glb";
            static const char* sponza = ASSET_PATH "mesh/sponza-gltf-pbr/sponza.glb";
            static const char* sunTemple = ASSET_PATH "mesh/sun_temple/simplified_sun_temple.glb";
            static const char* ueArchviz = ASSET_PATH "mesh/archviz.glb";
            static const char* cornellBox = ASSET_PATH "mesh/cornell_box.gltf";
            static const char* diorama = ASSET_PATH "mesh/sd_macross_diorama.glb";
            static const char* picapica = ASSET_PATH "mesh/pica_pica_scene.glb";

            AssetImporter::importAsync(m_scene.get(), ueArchviz);

            // skybox
            auto skybox = m_scene->createSkybox("Skybox", ASSET_PATH "cubemaps/neutral_sky.hdr", glm::uvec2(2048));
            // sun light
            m_scene->createDirectionalLight("SunLight", glm::vec3(0.3f, 1.3f, 0.5f), glm::vec4(0.88f, 0.77f, 0.65f, 30.f));
            // sky light 
            auto skylight = m_scene->createSkyLight("SkyLight", ASSET_PATH "cubemaps/neutral_sky.hdr");
            skylight->build();

            // overwrite the default rendering lambda
            m_renderOneFrame = [this](GfxTexture2D* renderingOutput) {
                auto renderer = Renderer::get();
                // scene rendering
                if (m_scene) 
                {
                    if (auto camera = dynamic_cast<PerspectiveCamera*>(m_scene->m_mainCamera->getCamera())) 
                    {
                        SceneView mainSceneView(*m_scene, *camera,
                            [](Entity* entity) {
                                return entity->getProperties() | EntityFlag_kVisible;
                            },
                            renderingOutput, 
                            { 0, 0, renderingOutput->width, renderingOutput->height }
                        );
                        renderer->render(m_scene.get(), mainSceneView, glm::uvec2(renderingOutput->width, renderingOutput->height));
                    }
                }
                renderer->renderToScreen(renderingOutput);

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
                        SceneInspectorPanel(Scene* scene)
                            : m_scene(scene)
                        {

                        }

                        void renderEntity(Entity* entity)
                        {
                            if (entity)
                            {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();

                                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;
                                if (selectedEntity && (selectedEntity->name == entity->name))
                                {
                                    flags |= ImGuiTreeNodeFlags_Selected;
                                }

                                if (!entity->childs.empty())
                                {
                                    bool open = ImGui::TreeNodeEx(entity->name.c_str(), flags);
                                    if (ImGui::IsItemClicked())
                                    {
                                        selectedEntity = entity;
                                    }
                                    ImGui::TableNextColumn();
                                    ImGui::TextColored(ImVec4(1.0, 1.0, 1.0, 0.2), entity->getTypeDesc());
                                    if (open)
                                    {
                                        for (auto child : entity->childs)
                                        {
                                            renderEntity(child);
                                        }
                                        ImGui::TreePop();
                                    }
                                }
                                else
                                {
                                    flags |= ImGuiTreeNodeFlags_Leaf;
                                    bool open = ImGui::TreeNodeEx(entity->name.c_str(), flags);
                                    if (ImGui::IsItemClicked())
                                    {
                                        selectedEntity = entity;
                                    }
                                    ImGui::TableNextColumn();
                                    ImGui::TextColored(ImVec4(1.0, 1.0, 1.0, 0.2), entity->getTypeDesc());
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
                            if (m_scene)
                            {
                                if (ImGui::BeginTable("##SceneHierarchy", 2, flags, ImVec2(ImGui::GetWindowContentRegionWidth(), 100.f)))
                                {
                                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                                    ImGui::TableSetupColumn("Type");
                                    ImGui::TableHeadersRow();

                                    renderEntity(m_scene->m_rootEntity);

                                    ImGui::EndTable();
                                }

                            }
                        }

                        Entity* selectedEntity = nullptr;
                        Scene* m_scene = nullptr;
                    };

                    static SceneInspectorPanel sceneInspector(m_scene.get());
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
                                            selected = (selectedVis->name == vis.name);
                                        }
                                        if (vis.texture) {
                                            if (ImGui::MenuItem(vis.name.c_str(), nullptr, selected)) {
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
                    if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::TextColored(ImVec4(0.f, 1.f, 1.f, 1.f), "Frame Time: %.2f ms", gEngine->getFrameElapsedTimeInMs());
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
                    if (ImGui::CollapsingHeader("Screen Space Ray Tracing", ImGuiTreeNodeFlags_DefaultOpen))
                    {
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
                        ImGui::Text("Spatial Reuse Kernel Radius:"); 
                        ImGui::SameLine();
                        ImGui::SliderFloat("##Spatial Reuse Kernel Radius:", &renderer->m_ssgi.reuseKernelRadius, 0.f, 1.f);

                        ImGui::Text("Spatial Reuse Sample Count:"); 
                        ImGui::SameLine();
                        ImGui::SliderInt("##Spatial Reuse Sample Count:", &renderer->m_ssgi.numReuseSamples, 1, 8);
                    }
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
                                ImGui::Text("%s", selectedVis->name);
                                ImGui::SliderInt("Active Mip:", &selectedVis->activeMip, 0, texture->numMips - 1);
                            }
                        }
                    }
                }
                ImGui::End();
            });
        }
    };
}

int main()
{
    std::unique_ptr<Cyan::Editor> editor = std::make_unique<Cyan::Editor>();
    editor->run();
    return 0;
}