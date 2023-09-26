#pragma once

#include "Core.h"
#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtc/integer.hpp"

#define M_PI 3.141592653589793f

namespace Cyan
{
    glm::vec3 vec4ToVec3(const glm::vec4& v4);
    CORE_API bool isPowerOf2(i32 value);
    CORE_API glm::vec3 uniformSampleHemisphere(const glm::vec3& n);
    CORE_API glm::vec3 uniformSampleCone(const glm::vec3& n, f32 halfConeAperture);
    CORE_API f32 uniformSampleZeroToOne();
    CORE_API f32 randomFloatBetween(f32 f1, f32 f2);
}