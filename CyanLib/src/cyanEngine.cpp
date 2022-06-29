#include <iostream>
#include <chrono>

#include "glew.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "Common.h"
#include "cyanRenderer.h"
#include "window.h"
#include "Shader.h"
#include "Scene.h"
#include "camera.h"
#include "mathUtils.h"
#include "cyanEngine.h"


namespace Cyan
{
    Engine* Engine::singleton = nullptr;

    Engine::Engine(u32 windowWidth, u32 windowHeight)
    {
        if (!singleton)
        {
            singleton = this;
            m_graphicsSystem = std::make_unique<GraphicsSystem>(windowWidth, windowHeight);
            m_IOSystem = std::make_unique<IOSystem>();
        }
        else
        {
            CYAN_ASSERT(0, "Attempted to instantiate multiple instances of Engine")
        }
    }

    void Engine::initialize()
    {
        m_graphicsSystem->initialize();
        m_IOSystem->initialize();

        m_renderer = m_graphicsSystem->getRenderer();
    }
    
    void Engine::update()
    {
        // todo: gather frame statistics

        // update window title
        static u32 numFrames = 0u;
        char windowTitle[64] = { };
        sprintf_s(windowTitle, "Cyan | Frame: %d | FPS: %.2f", numFrames++, 60.0f);
        glfwSetWindowTitle(m_graphicsSystem->getAppWindow(), windowTitle);

        m_IOSystem->update();
        m_graphicsSystem->setScene(m_scene);
        m_graphicsSystem->update();

        /**     
        * render utility widgets(e.g: such as a scene outline window, entity details window)
        */ 
        m_renderer->addUIRenderCommand([this]() {
            ImGuiWindowFlags flags = ImGuiWindowFlags_None;
            ImGui::SetNextWindowPos(ImVec2(5, 5));
            ImGui::SetNextWindowSize(ImVec2(360, 700));
            ImGui::Begin("Cyan", nullptr, flags);
            {
                ImGui::BeginChild("##SceneView", ImVec2(0, 0), true);
                static ImGuiTableFlags flags =
                     ImGuiTableFlags_Resizable
                    | ImGuiTableFlags_RowBg
                    | ImGuiTableFlags_NoBordersInBody;

                /**
                * Scene outline widget
                */
                struct EntityTable
                {
                    EntityTable(Entity* inEntity) : root(inEntity) { }

                    void render()
                    {
                        Entity* highlightedEntity = nullptr;
                        ImGui::TextUnformatted("Scene");
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

                    Entity* root = nullptr;
                    Entity* highlightedEntity = nullptr;
                }; 

                static EntityTable entityTable(m_scene->rootEntity);
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

                                    SceneComponent* render()
                                    {
                                        SceneComponent* highlightedComponent = nullptr;
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

                                    static SceneComponent* renderComponent(SceneComponent* component)
                                    {
                                        SceneComponent* highlightedComponent = nullptr;
                                        if (component)
                                        {
                                            ImGui::TableNextRow();
                                            ImGui::TableNextColumn();
                                            if (!component->childs.empty())
                                            {
                                                bool open = ImGui::TreeNodeEx(component->name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                                                ImGui::TableNextColumn();
                                                ImGui::TextColored(ImVec4(1.0, 1.0, 1.0, 0.2), "SceneComponent");
                                                if (open)
                                                {
                                                    for (auto child : component->childs)
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
                ImGui::EndChild();
            }
            ImGui::End();
        });

        // tick
        for (auto entity : m_scene->entities)
        {
            entity->update();
        }
    }

    void Engine::render()
    {
        if (m_renderer)
        {
            m_renderer->render(m_scene.get());
        }
    }

    void Engine::finalize()
    {
        m_IOSystem->finalize();
        m_graphicsSystem->finalize();
    }
}