#pragma once

#include "Core.h"
#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtc/integer.hpp"

namespace Cyan
{
    glm::vec3 vec4ToVec3(const glm::vec4& v4);
    bool CORE_API isPowerOf2(i32 value);
}