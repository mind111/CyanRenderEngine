#pragma once

#include "glm.hpp"

#include "Component.h"
#include "Entity.h"
#include "Texture.h"

namespace Cyan
{
    struct Light
    {
        glm::vec4 colorAndIntensity;

        glm::vec3 getColor() { return glm::vec3(colorAndIntensity.r, colorAndIntensity.g, colorAndIntensity.b); }
        float getIntensity() { return colorAndIntensity.a; }
        void setColor(const glm::vec4& colorAndIntensity) { }
    };
}
