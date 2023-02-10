#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "StaticMeshEntity.h"
#include "Scene.h"
#include "Material.h"

namespace Cyan
{
    StaticMeshEntity::StaticMeshEntity(Scene* scene, const char* inName, const Transform& t, StaticMesh* inMesh, Entity* inParent, u32 inProperties)
        : Entity(scene, inName, t, inParent, inProperties)
    {
        staticMeshComponent = std::make_unique<MeshComponent>(this, "StaticMeshComponent", Transform{ }, inMesh);
        attachComponent(staticMeshComponent.get());
    }

    void StaticMeshEntity::renderUI()
    {
        /**
        * Render a widget to edit meshInstance related properties
        */
        if (ImGui::CollapsingHeader("StaticMeshEntity", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto meshInst = staticMeshComponent->getAttachedMesh();
            auto mesh = meshInst->mesh;

            // todo: mesh thumbnail
            ImGui::Text("Mesh"); ImGui::SameLine();
            if (ImGui::BeginCombo("##Mesh", mesh->name.c_str()))
            {
                ImGui::EndCombo();
            }

            ImGui::Separator();

            // todo: material thumbnail
            // left
            static int selected = 0;
            {
                ImGui::BeginChild("Material List View", ImVec2(150, 300), true);
                for (int i = 0; i < mesh->numSubmeshes(); i++)
                {
                    char label[128];
                    sprintf(label, "Material %d", i);
                    if (ImGui::Selectable(label, selected == i))
                        selected = i;
                }
                ImGui::EndChild();
            }
            ImGui::SameLine();

            // right
            {
                auto material = meshInst->getMaterial(selected);
                ImGui::BeginGroup();
                {
                    ImGui::BeginChild("Material Details View", ImVec2(0, 300)); // Leave room for 1 line below us
                    {
                        ImGui::Text("Material %d: %s", selected, material->name.c_str());
                        ImGui::Separator();
                    }
                    ImGui::EndChild();
                }
                ImGui::EndGroup();
            }
        }
    }

    void StaticMeshEntity::setMaterial(Material* material) {
        staticMeshComponent->setMaterial(material);
    }

    void StaticMeshEntity::setMaterial(Material* material, u32 index) {
        staticMeshComponent->setMaterial(material, index);
    }
}