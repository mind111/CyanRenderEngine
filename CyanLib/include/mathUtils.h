#pragma once

#include <iostream>

#include "Common.h"
#include <cmath>
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "mat4x4.hpp"

#define M_PI 3.141592653589793f
#define INV_PI 0.31830988f

#define RADIANS(x) (x) * 2 * M_PI / 360.f

class MathUtils {
public:
    static glm::mat4 transformToMat4(const struct Transform& xform);
};

namespace Cyan
{
    f32 fabs(f32 value);
    glm::vec3 vec4ToVec3(const glm::vec4& v4);
    glm::mat4 calcTangentFrame(const glm::vec3& n);
    glm::mat3 tangentToWorld(const glm::vec3& n);
    glm::vec3 sphericalToCartesian(const glm::vec3& n, f32 theta, f32 phi);
    f32 uniformSampleZeroToOne();
    glm::vec3 uniformSampleHemisphere(const glm::vec3& n);
    glm::vec3 cosineWeightedSampleHemisphere(glm::vec3& n);
    glm::vec3 stratifiedCosineWeightedSampleHemiSphere(glm::vec3& normal, f32 j, f32 k, f32 M, f32 N);
    glm::vec2 halton23(u32 index);

    // vector math
    inline f32 dot(const glm::vec3& v0, const glm::vec3& v1)
    {
        return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
    }

    struct Vec3 
    {
        f32 x, y, z;
    };

    Vec3 operator+(const Vec3& lhs, const Vec3& rhs);
    Vec3 operator-(const Vec3& lhs, const Vec3& rhs);
    Vec3 operator*(const Vec3& lhs, const Vec3& rhs);
    Vec3 operator/(const Vec3& lhs, const Vec3& rhs);
}
