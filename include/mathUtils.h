#pragma once

#include "Common.h"
#include <cmath>
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "mat4x4.hpp"

#define M_PI 3.1415926f
#define RADIANS(x) (x) * 2 * M_PI / 360.f

class MathUtils {
public:
    static glm::mat4 transformToMat4(const struct Transform& xform);
};

namespace Cyan
{
    f32 fabs(f32 value);
}