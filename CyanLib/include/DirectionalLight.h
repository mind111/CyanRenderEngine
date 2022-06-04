#pragma once

#include "Entity.h"
#include "Texture.h"
#include "Component.h"
#include "Shadow.h"

namespace Cyan
{
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

        glm::vec3 direction;
        glm::vec4 colorAndIntensity;
        bool bCastShadow = false;
    };

#if 0
    struct DirectionalLightEntity : public Entity
    {
        DirectionalLightEntity()
            : Entity()
        {

        }
    };
#endif
}
