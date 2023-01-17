#include <thread>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "CyanApp.h"
#include "Scene.h"
#include "RayTracingScene.h"

class TheGame : public Cyan::DefaultApp
{
public:
    TheGame(u32 appWindowWidth, u32 appWindowHeight)
        : DefaultApp(appWindowWidth, appWindowHeight)
    {

    }

    ~TheGame() { }

    virtual void customInitialize() override
    {
        using namespace Cyan;

        {
            glm::uvec2 resolution = gEngine->getGraphicsSystem()->getAppWindowDimension();
            ITextureRenderable::Spec spec = { };
            spec.width = resolution.x;
            spec.height = resolution.y;
            spec.type = TEX_2D;
            spec.pixelFormat = PF_RGB16F;
            m_sceneRenderingOutput = new Texture2DRenderable("FrameOutput", spec);
        }

        static const char* shaderBalls = ASSET_PATH "mesh/shader_ball.glb";
        static const char* sponza = ASSET_PATH "mesh/sponza-gltf-pbr/sponza.glb";
        static const char* sunTemple = ASSET_PATH "mesh/sun_temple/simplified_sun_temple.glb";

        AssetManager::importGltf(m_scene.get(), shaderBalls);

        // skybox
        auto skybox = m_scene->createSkybox("Skybox", ASSET_PATH "cubemaps/neutral_sky.hdr", glm::uvec2(2048));
        // sun light
        m_scene->createDirectionalLight("SunLight", glm::vec3(1.3f, 1.3f, 0.5f), glm::vec4(0.88f, 0.77f, 0.65f, 50.f));
        // sky light 
        auto skylight = m_scene->createSkyLight("SkyLight", ASSET_PATH "cubemaps/neutral_sky.hdr");
        skylight->build();
    }

    virtual void customFinalize() override
    {
        for (auto thread : spawnedThreads)
        {
            if (thread)
            {
                thread->join();
                delete thread;
            }
        }
    }

    virtual void customUpdate() override
    {
    }

    static void drawSceneTab(Cyan::Scene* scene)
    {
        using namespace Cyan;

        ImGui::BeginChild("##SceneView", ImVec2(0, 0), true);
        {
            static ImGuiTableFlags flags =
                 ImGuiTableFlags_Resizable
                | ImGuiTableFlags_RowBg
                | ImGuiTableFlags_NoBordersInBody;

            /**
            * Scene outline widget
            */
            struct EntityTable
            {
                EntityTable(Cyan::Scene* inScene, Entity* inEntity) : scene(inScene), root(inEntity) { }

                void render()
                {
                    Entity* highlightedEntity = nullptr;
                    ImGui::Text(scene->m_name.c_str());
                    if (ImGui::BeginTable("##SceneTable", 2, flags, ImVec2(ImGui::GetWindowContentRegionWidth(), 100.f)))
                    {
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                        ImGui::TableSetupColumn("Type");
                        ImGui::TableHeadersRow();

                        renderEntity(root);

                        ImGui::EndTable();
                    }
                }

                void renderEntity(Entity* entity)
                {
                    if (entity)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnArrow;
                        if (highlightedEntity && (highlightedEntity->name == entity->name))
                        {
                            flags |= ImGuiTreeNodeFlags_Selected;
                        }

                        if (!entity->childs.empty())
                        {
                            bool open = ImGui::TreeNodeEx(entity->name.c_str(), flags);
                            if (ImGui::IsItemClicked())
                            {
                                highlightedEntity = entity;
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
                                highlightedEntity = entity;
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

                Cyan::Scene* scene = nullptr;
                Entity* root = nullptr;
                Entity* highlightedEntity = nullptr;
            }; 

            // todo: if we want to support changing active scene dynamically, then this cannot be static 
            static EntityTable entityTable(scene, scene->m_rootEntity);
            auto highlightedEntity = entityTable.highlightedEntity;
            if (ImGui::CollapsingHeader("Scene Layout", ImGuiTreeNodeFlags_DefaultOpen))
            {
                entityTable.render();
            }

            if (highlightedEntity)
            {
                ImGui::BeginTabBar("##Tabs");
                {
                    /**
                    * Entity's details tab
                    */
                    if (ImGui::BeginTabItem("Details"))
                    {
                        /**
                        * SceneComponent section
                        */
                        if (ImGui::CollapsingHeader("SceneComponent", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            struct SceneComponentTable
                            {
                                SceneComponentTable(Entity* inOwner) : owner(inOwner) { }

                                void setOwnerEntity(Entity* inOwner)
                                {
                                    owner = inOwner;
                                }

                                Component* render()
                                {
                                    Component* highlightedComponent = nullptr;
                                    ImGui::Text("%s", owner->name.c_str());
                                    if (ImGui::BeginTable("##SceneComponentTable", 2, flags, ImVec2(ImGui::GetWindowContentRegionWidth(), 100.f)))
                                    {
                                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
                                        ImGui::TableSetupColumn("Type");
                                        ImGui::TableHeadersRow();

                                        highlightedComponent = renderComponent(owner->getRootSceneComponent());

                                        ImGui::EndTable();
                                    }
                                    return highlightedComponent;
                                }

                                static Component* renderComponent(Component* component)
                                {
                                    Component* highlightedComponent = nullptr;
                                    if (component)
                                    {
                                        ImGui::TableNextRow();
                                        ImGui::TableNextColumn();
                                        if (!component->children.empty())
                                        {
                                            bool open = ImGui::TreeNodeEx(component->name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                                            ImGui::TableNextColumn();
                                            ImGui::TextColored(ImVec4(1.0, 1.0, 1.0, 0.2), "SceneComponent");
                                            if (open)
                                            {
                                                for (auto child : component->children)
                                                {
                                                    renderComponent(child);
                                                }
                                                ImGui::TreePop();
                                            }
                                        }
                                        else
                                        {
                                            bool open = ImGui::TreeNodeEx(component->name.c_str(), ImGuiTreeNodeFlags_Leaf);
                                            ImGui::TableNextColumn();
                                            ImGui::TextColored(ImVec4(1.0, 1.0, 1.0, 0.2), "SceneComponent");
                                            if (open)
                                            {
                                                ImGui::TreePop();
                                            }
                                        }
                                    }
                                    return highlightedComponent;
                                }

                                Entity* owner = nullptr;
                            };
                            static SceneComponentTable sceneComponentTable(highlightedEntity);
                            sceneComponentTable.setOwnerEntity(highlightedEntity);
                            sceneComponentTable.render();
                        }

                        /**
                        * Transform data section
                        */
                        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGui::TextUnformatted("Position"); ImGui::SameLine();
                            glm::vec3 worldPosition = highlightedEntity->getWorldPosition();
                            f32 position[3] = {
                                worldPosition.x,
                                worldPosition.y,
                                worldPosition.z
                            };
                            f32 rotation[3] = {
                                0.f, 0.f, 0.f
                            };
                            ImGui::InputFloat3("##Position", position);
                            f32 scale[3] = {
                                1.f, 1.f, 1.f
                            };
                            ImGui::TextUnformatted("Rotation"); ImGui::SameLine();
                            ImGui::InputFloat3("##Rotation", rotation);
                            ImGui::SetNextItemWidth(ImGui::CalcTextSize("Rotation").x);
                            ImGui::TextUnformatted("Scale"); ImGui::SameLine();
                            ImGui::InputFloat3("##Scale", scale);
                            ImGui::Separator();
                        }

                        /**
                        * Entity's custom data section
                        */
                        highlightedEntity->renderUI();

                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();
    }

    static void drawRenderingTab(Cyan::Renderer* renderer, f32 renderFrameTime)
    {
        ImGui::BeginChild("##Rendering Settings", ImVec2(0, 0), true, ImGuiWindowFlags_MenuBar);
        {
            // todo: this pointer is unsafe as it points to an element in a vector which will
            // likely be changed when the vector is resized.
            static Cyan::Renderer::VisualizationDesc* selectedVis = nullptr;
            // rendering main menu
            if (ImGui::BeginMenuBar()) {
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
                                    selected = (selectedVis->texture->name == vis.texture->name);
                                }
                                if (vis.texture) {
                                    if (ImGui::MenuItem(vis.texture->name, nullptr, selected)) {
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
                ImGui::TextColored(ImVec4(0.f, 1.f, 1.f, 1.f), "Frame Time: %.2f ms", renderFrameTime);
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
                        ImGui::Text("%s", texture->name);
                        ImGui::SliderInt("Active Mip:", &selectedVis->activeMip, 0, texture->numMips - 1);
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    virtual void customRender() override
    {
        /**     
        * render utility widgets(e.g: such as a scene outline window, entity details window)
        */ 
        gEngine->getGraphicsSystem()->getRenderer()->addUIRenderCommand([this]() {
            ImGuiWindowFlags flags = ImGuiWindowFlags_None;
            ImGui::SetNextWindowPos(ImVec2(5, 5));
            ImGui::SetNextWindowSize(ImVec2(360, 700));

            ImGui::Begin("Cyan", nullptr, ImGuiWindowFlags_NoCollapse);
            {
                ImGui::BeginTabBar("##Views");
                {
                    if (ImGui::BeginTabItem("Scene"))
                    {
                        drawSceneTab(m_scene.get());
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Rendering"))
                    {
                        drawRenderingTab(gEngine->getGraphicsSystem()->getRenderer(), 0.f);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
        });
    }

private:
    std::unique_ptr<Cyan::RayTracingScene> m_rtxScene = nullptr;
    std::unique_ptr<Cyan::RayTracer> m_rayTracer = nullptr;
    std::vector<std::thread*> spawnedThreads;
    Cyan::Image image = Cyan::Image(glm::uvec2(640, 360));
    Cyan::Texture2DRenderable* rayTracingOutput = nullptr;
};

// entry point
int main()
{
    TheGame* app = new TheGame(1024, 1024);
    app->initialize();
    app->run();
    app->deinitialize();
    delete app;
    return 0;
}
