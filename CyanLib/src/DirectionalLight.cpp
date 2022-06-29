#include "DirectionalLight.h"
#include "CyanRenderer.h"
#include "Scene.h"
#include "LightComponent.h"
#include "CyanRenderer.h"
#include "CyanUI.h"

namespace Cyan
{
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic)
    {
        directionalLightComponentPtr = std::make_unique<DirectionalLightComponent>();
        addComponent(directionalLightComponentPtr.get());
    }
    
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent, const glm::vec3& direction, const glm::vec4& colorAndIntensity, bool bCastShadow)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic)
    {
        directionalLightComponentPtr = std::make_unique<DirectionalLightComponent>(direction, colorAndIntensity, bCastShadow);
        addComponent(directionalLightComponentPtr.get());
    }

    void DirectionalLightEntity::update()
    {
    }

    void DirectionalLightEntity::renderUI()
    {
        if (ImGui::CollapsingHeader("DirectionalLight", ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec4 colorAndIntensity = directionalLightComponentPtr->getColorAndIntensity();
            // color
            ImGui::Text("Color"); ImGui::SameLine();
            f32 color[3] = { colorAndIntensity.x, colorAndIntensity.y, colorAndIntensity.z };
            ImGui::ColorPicker3("##Color", color);
            colorAndIntensity.r = color[0];
            colorAndIntensity.g = color[1];
            colorAndIntensity.b = color[2];

            // intensity
            ImGui::Text("Intensity"); ImGui::SameLine();
            ImGui::SliderFloat("##Intensity", &colorAndIntensity.w, 0.f, 100.f);
            directionalLightComponentPtr->setColorAndIntensity(colorAndIntensity);
        }
    }
}
