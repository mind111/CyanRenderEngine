#pragma once

#include "Common.h"
#include "glm/glm.hpp"

namespace Cyan
{
    struct PointLight
    {
        glm::vec3 position;
        glm::vec4 colorAndIntensity;
    };
}
