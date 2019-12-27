#pragma once

#include <cmath>
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "mat4x4.hpp"

class MathUtils {
public:
    static glm::mat4 transformToMat4(const struct Transform& xform);
};