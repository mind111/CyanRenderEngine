#include "DirectionalLight.h"
#include "CyanRenderer.h"
#include "Scene.h"
#include "LightComponent.h"
#include "CyanRenderer.h"
#include "CyanUI.h"

namespace Cyan
{
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic | EntityFlag_kVisible)
    {
        directionalLightComponentPtr = std::make_unique<DirectionalLightComponent>();
        addComponent(directionalLightComponentPtr.get());
    }
    
    DirectionalLightEntity::DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent, const glm::vec3& direction, const glm::vec4& colorAndIntensity, bool bCastShadow)
        : Entity(scene, inName, t, inParent, EntityFlag_kDynamic | EntityFlag_kVisible)
    {
        directionalLightComponentPtr = std::make_unique<DirectionalLightComponent>(direction, colorAndIntensity, bCastShadow);
        addComponent(directionalLightComponentPtr.get());
    }

    void DirectionalLightEntity::upload()
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

    // helper function for converting world space AABB to light's view space
    BoundingBox3D calcLightSpaceAABB(const glm::vec3& inLightDirection, const BoundingBox3D& worldSpaceAABB)
    {
        // derive the 8 vertices of the aabb from pmin and pmax
        // d -- c
        // |    |
        // a -- b
        // f stands for front while b stands for back
        f32 width = worldSpaceAABB.pmax.x - worldSpaceAABB.pmin.x;
        f32 height = worldSpaceAABB.pmax.y - worldSpaceAABB.pmin.y;
        f32 depth = worldSpaceAABB.pmax.z - worldSpaceAABB.pmin.z;
        glm::vec3 fa = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, -height, 0.f);
        glm::vec3 fb = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(0.f, -height, 0.f);
        glm::vec3 fc = vec4ToVec3(worldSpaceAABB.pmax);
        glm::vec3 fd = vec4ToVec3(worldSpaceAABB.pmax) + glm::vec3(-width, 0.f, 0.f);

        glm::vec3 ba = fa + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bb = fb + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bc = fc + glm::vec3(0.f, 0.f, -depth);
        glm::vec3 bd = fd + glm::vec3(0.f, 0.f, -depth);

        glm::mat4 lightSpaceView = glm::lookAt(glm::vec3(0.f), -inLightDirection, glm::vec3(0.f, 1.f, 0.f));
        BoundingBox3D lightSpaceAABB = { };
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fa, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(fd, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(ba, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bb, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bc, 1.f));
        lightSpaceAABB.bound(lightSpaceView * glm::vec4(bd, 1.f));
        return lightSpaceAABB;
    }
}
