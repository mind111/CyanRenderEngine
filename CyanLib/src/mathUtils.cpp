#include "mathUtils.h"
#include "scene.h"

glm::mat4 MathUtils::transformToMat4(const Transform& xform) {
    glm::mat4 res(1.f);
    res = glm::translate(res, xform.m_translate);
    res = glm::scale(res, xform.m_scale);
    return res;
}

namespace Cyan
{
    f32 fabs(f32 value)
    {
        return abs(value);
    }

    glm::vec3 vec4ToVec3(const glm::vec4& v4)
    {
        return glm::vec3(v4.x, v4.y, v4.z);
    }

    glm::mat3 tangentToWorld(const glm::vec3& n)
    {
        glm::vec3 worldUp = abs(n.y) < 0.95f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(0.f, 0.f, 1.f);
        glm::vec3 right = glm::cross(n, worldUp);
        glm::vec3 forward = glm::cross(n, right);
        glm::mat3 coordFrame = {
            right,
            forward,
            n
        };
        return coordFrame;
    }

    f32 uniformSampleZeroToOne()
    {
        return rand() / (f32)RAND_MAX;
    }

    glm::vec3 uniformSampleHemiSphere(glm::vec3& n)
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

    glm::vec2 uniformSampleUnitDisk()
    {
        f32 u = uniformSampleZeroToOne();
        f32 v = uniformSampleZeroToOne();
        f32 r = sqrt(u);
        f32 theta = 2 * M_PI * v;
        return glm::vec2(r * cos(theta), r * sin(theta));
    }

    glm::vec3 cosineWeightedSampleHemiSphere(glm::vec3& n)
    {
        glm::vec2 pDisk = uniformSampleUnitDisk();
        f32 pz = 1.f - sqrt(pDisk.x * pDisk.x + pDisk.y * pDisk.y);
        glm::vec3 localDir(pDisk, pz);
        return tangentToWorld(n) * localDir; 
    }

    f32 dot(const glm::vec3& v0, const glm::vec3& v1)
    {
        return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
    }

    Vec3 operator+(const Vec3& lhs, const Vec3& rhs)
    {
        return Vec3{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    }  

    Vec3 operator-(const Vec3& lhs, const Vec3& rhs)
    {
        return Vec3{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }  

    Vec3 operator*(const Vec3& lhs, const Vec3& rhs)
    {
        return Vec3{lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z};
    }  

    Vec3 operator/(const Vec3& lhs, const Vec3& rhs)
    {
        return Vec3{lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z};
    }
}