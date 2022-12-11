#include "StaticMeshEntity.h"
#include "Scene.h"
#include "Material.h"
#include "CyanUI.h"

namespace Cyan
{
    StaticMeshEntity::StaticMeshEntity(Scene* scene, const char* inName, const Transform& t, Mesh* inMesh, Entity* inParent, u32 inProperties)
        : Entity(scene, inName, t, inParent, inProperties)
    {
        meshComponentPtr = std::unique_ptr<MeshComponent>(scene->createMeshComponent(inMesh, Transform{ }));
        attachSceneComponent(meshComponentPtr.get());
        addComponent(meshComponentPtr.get());
    }

    void StaticMeshEntity::renderUI()
    {
        /**
        * Render a widget to edit meshInstance related properties
        */
        if (ImGui::CollapsingHeader("StaticMeshEntity", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto meshInst = meshComponentPtr->getAttachedMesh();
            auto mesh = meshInst->parent;

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
                        material->renderUI();
                    }
                    ImGui::EndChild();
                }
                ImGui::EndGroup();
            }
        }
    }

    void StaticMeshEntity::setMaterial(Material* material) {
        meshComponentPtr->setMaterial(material);
    }

    void StaticMeshEntity::setMaterial(Material* material, u32 index) {
        meshComponentPtr->setMaterial(material, index);
    }
}