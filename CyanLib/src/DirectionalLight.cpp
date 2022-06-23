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
        auto renderer = Renderer::get();
        renderer->addUIRenderCommand([this]() {
            ImGuiWindowFlags flags = ImGuiWindowFlags_None | ImGuiWindowFlags_NoResize;
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGui::SetNextWindowPos(ImVec2(10, 10));
            ImGui::SetNextWindowSize(ImVec2(270, 400));
            ImGui::Begin("DirectionalLight", nullptr, flags);
            {
                glm::vec4 colorAndIntensity = directionalLightComponentPtr->getColorAndIntensity();
                ImGui::Text("Intensity"); ImGui::SameLine();
                ImGui::SliderFloat("##Intensity", &colorAndIntensity.w, 0.f, 100.f);
                directionalLightComponentPtr->setColorAndIntensity(colorAndIntensity);
            }
            ImGui::End();
        });
    }
}
