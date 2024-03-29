#include "Entity.h"
#include "LightEntities.h"
#include "LightComponents.h"
#include "CyanRenderer.h"
#include "Scene.h"
#include "CyanRenderer.h"
#include "CyanUI.h"

namespace Cyan
{
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic | EntityFlag_kVisible) {
        directionalLightComponent = std::make_unique<DirectionalLightComponent>();
        addComponent(directionalLightComponent.get());
    }
    
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent, const glm::vec3& direction, const glm::vec4& colorAndIntensity, bool bCastShadow)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic | EntityFlag_kVisible) {
        directionalLightComponent = std::make_unique<DirectionalLightComponent>(direction, colorAndIntensity, bCastShadow);
        addComponent(directionalLightComponent.get());
    }

    void DirectionalLightEntity::update()
    {
    }

    void DirectionalLightEntity::renderUI()
    {
        if (ImGui::CollapsingHeader("DirectionalLight", ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec4 colorAndIntensity = directionalLightComponent->getColorAndIntensity();
            // albedo
            ImGui::Text("Color"); ImGui::SameLine();
            f32 albedo[3] = { colorAndIntensity.x, colorAndIntensity.y, colorAndIntensity.z };
            ImGui::ColorPicker3("##Color", albedo);
            colorAndIntensity.r = albedo[0];
            colorAndIntensity.g = albedo[1];
            colorAndIntensity.b = albedo[2];

            // intensity
            ImGui::Text("Intensity"); ImGui::SameLine();
            ImGui::SliderFloat("##Intensity", &colorAndIntensity.w, 0.f, 100.f);
            directionalLightComponent->setColorAndIntensity(colorAndIntensity);
        }
    }
}
