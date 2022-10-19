#pragma once

#include "Entity.h"
#include "Texture.h"
#include "Component.h"
#include "Shadow.h"

namespace Cyan
{
    struct DirectionalLightComponent;

    struct DirectionalLight
    {
        enum class ShadowQuality
        {
            kLow,
            kMedium,
            kHigh,
            kInvalid
        } shadowQuality = ShadowQuality::kHigh;

        enum class Implementation
        {
            kBasic,
            kVarianceShadowmap,
            kCSM_Basic,
            kCSM_VarianceShadowmap,
            kCount
        } implemenation = Implementation::kCSM_Basic;

        DirectionalLight() { }
        DirectionalLight(const glm::vec3& inDirection, const glm::vec4& inColorAndIntensity, bool inCastShadow)
            : direction(glm::normalize(inDirection)), colorAndIntensity(inColorAndIntensity), bCastShadow(inCastShadow)
        { 
            /** note:
                assuming that input illuminance is specified in lux, and remap 90000 lux (illuminance in normal direction of a midday sun) to x intensity unit.
            */
#if 0
            const float luxToIntensity = 1.0 / 90000.0;
            colorAndIntensity.w *= luxToIntensity;
#endif
        }

        glm::vec3 direction = glm::normalize(glm::vec3(1.f, 1.f, 1.f));
        glm::vec4 colorAndIntensity = glm::vec4(1.f, 0.7f, 0.9f, 1.f);
        bool bCastShadow = true;
    };

    struct DirectionalLightEntity : public Entity
    {
        /* Entity interface */
        virtual void update() override;
        virtual const char* getTypeDesc() override { return "DirectionalLightEntity"; }
        virtual void renderUI() override;

        DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent);
        DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParenat, const glm::vec3& direction, const glm::vec4& colorAndIntensity, bool bCastShadow);
    private:
        std::unique_ptr<DirectionalLightComponent> directionalLightComponentPtr = nullptr;
    };

    // helper function for converting world space AABB to light's view space
    BoundingBox3D calcLightSpaceAABB(const glm::vec3& inLightDirection, const BoundingBox3D& worldSpaceAABB);
}
