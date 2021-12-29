#pragma once

#include "Common.h"
#include <cmath>
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "mat4x4.hpp"

#define M_PI 3.141592653589793f

#define RADIANS(x) (x) * 2 * M_PI / 360.f

class MathUtils {
public:
    static glm::mat4 transformToMat4(const struct Transform& xform);
};

namespace Cyan
{
    f32 fabs(f32 value);
    glm::vec3 vec4ToVec3(const glm::vec4& v4);
    glm::mat3 tangentToWorld(const glm::vec3& n);
    f32 uniformSampleZeroToOne();
    glm::vec3 uniformSampleHemiSphere(glm::vec3& n);
    glm::vec3 cosineWeightedSampleHemiSphere(glm::vec3& n);

    struct Vec3 
    {
        f32 x, y, z;
    };

    Vec3 operator+(const Vec3& lhs, const Vec3& rhs);
    Vec3 operator-(const Vec3& lhs, const Vec3& rhs);
    Vec3 operator*(const Vec3& lhs, const Vec3& rhs);
    Vec3 operator/(const Vec3& lhs, const Vec3& rhs);
}
