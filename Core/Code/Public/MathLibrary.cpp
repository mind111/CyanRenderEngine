#include "MathLibrary.h"

namespace Cyan
{
    glm::vec3 vec4ToVec3(const glm::vec4& v4)
    {
        return glm::vec3(v4.x, v4.y, v4.z);
    }

    bool isPowerOf2(i32 value)
    {
        return (value & (value - 1)) == 0;
    }

    f32 uniformSampleZeroToOne()
    {
        return rand() / (f32)RAND_MAX;
    }

    glm::mat3 tangentToWorld(const glm::vec3& n)
    {
        glm::vec3 m_worldUp = abs(n.y) < 0.99f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(0.f, 0.f, 1.f);
        glm::vec3 right = glm::cross(n, m_worldUp);
        glm::vec3 forward = glm::cross(n, right);
        glm::mat3 coordFrame = {
            right,
            forward,
            n
        };
        return coordFrame;
    }

    glm::vec3 sphericalToCartesian(const glm::vec3& n, f32 theta, f32 phi)
    {
        glm::vec3 localDir = {
            sin(theta) * cos(phi),
            sin(theta) * sin(phi),
            cos(theta)
        };
        return tangentToWorld(n) * localDir;
    }

    glm::vec3 uniformSampleHemisphere(const glm::vec3& n)
    {
        f32 r0 = uniformSampleZeroToOne();
        f32 r1 = uniformSampleZeroToOne();
        f32 theta = acosf(r0);
        f32 phi = 2 * M_PI * r1;
        glm::vec3 localDir = {
            sin(theta) * cos(phi),
            sin(theta) * sin(phi),
            cos(theta)
        };
        return tangentToWorld(n) * localDir; 
    }

    CORE_API glm::vec3 uniformSampleCone(const glm::vec3& n, f32 halfConeAperture)
    {
        f32 t = halfConeAperture / 90.f;

        f32 r0 = uniformSampleZeroToOne();
        f32 r1 = uniformSampleZeroToOne();
        f32 theta = acosf(r0) * t;
        f32 phi = 2 * M_PI * r1;
        glm::vec3 localDir = {
            sin(theta) * cos(phi),
            sin(theta) * sin(phi),
            cos(theta)
        };
        return tangentToWorld(n) * localDir; 
    }

    CORE_API f32 randomFloatBetween(f32 f1, f32 f2)
    {
        f32 v0 = glm::min(f1, f2);
        f32 v1 = glm::max(f1, f2);
        f32 t = uniformSampleZeroToOne();
        return v0 + t * (v1 - v0);
    }
}