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
            kCSM,
            kVSM,
            kCount
        } implemenation = Implementation::kCSM;

        DirectionalLight() { }
        DirectionalLight(const glm::vec3& inDirection, const glm::vec4& inColorAndIntensity, bool inCastShadow)
            : direction(inDirection), colorAndIntensity(inColorAndIntensity), bCastShadow(inCastShadow)
        { }

        glm::vec3 direction = glm::normalize(glm::vec3(1.f, 1.f, 1.f));
        glm::vec4 colorAndIntensity = glm::vec4(1.f, 0.7f, 0.9f, 1.f);
        bool bCastShadow = false;
    };

    struct DirectionalLightEntity : public Entity
    {
        /* Entity interface */
        virtual void update() override { }

        DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParent);
        DirectionalLightEntity(Scene* scene, const char* inName, const Transform& t, Entity* inParenat, const glm::vec3& direction, const glm::vec4& colorAndIntensity, bool bCastShadow);
    };
}
