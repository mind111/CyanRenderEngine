#include "Geometry.h"
#include "CyanAPI.h"
#include "VertexArray.h"
#include "MathUtils.h"

namespace Cyan
{
    Triangles::Triangles(const std::vector<Triangles::Vertex>& inVertices, const std::vector<u32>& inIndices)
        : vertices(inVertices), indices(inIndices)
    {

    }

    Lines::Lines(const std::vector<Lines::Vertex>& inVertices, const std::vector<u32>& inIndices)
        : vertices(inVertices), indices(inIndices)
    {

    }

}

BoundingBox3D::BoundingBox3D()
{
    pmin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    pmax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
}

void BoundingBox3D::reset()
{
    pmin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    pmax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
}

void BoundingBox3D::bound(const BoundingBox3D& aabb) 
{
    pmin.x = Min(pmin.x, aabb.pmin.x);
    pmin.y = Min(pmin.y, aabb.pmin.y);
    pmin.z = Min(pmin.z, aabb.pmin.z);

    pmax.x = Max(pmax.x, aabb.pmax.x);
    pmax.y = Max(pmax.y, aabb.pmax.y);
    pmax.z = Max(pmax.z, aabb.pmax.z);
}

void BoundingBox3D::bound(const glm::vec3& v3)
{
    pmin.x = Min(pmin.x, v3.x);
    pmin.y = Min(pmin.y, v3.y);
    pmin.z = Min(pmin.z, v3.z);
    pmax.x = Max(pmax.x, v3.x);
    pmax.y = Max(pmax.y, v3.y);
    pmax.z = Max(pmax.z, v3.z);
}

void BoundingBox3D::bound(const glm::vec4& v4)
{
    pmin.x = Min(pmin.x, v4.x);
    pmin.y = Min(pmin.y, v4.y);
    pmin.z = Min(pmin.z, v4.z);
    pmax.x = Max(pmax.x, v4.x);
    pmax.y = Max(pmax.y, v4.y);
    pmax.z = Max(pmax.z, v4.z);
}

void BoundingBox3D::bound(const Triangle& tri)
{
    bound(tri.m_vertices[0]);
    bound(tri.m_vertices[1]);
    bound(tri.m_vertices[2]);
}

inline f32 ffmin(f32 a, f32 b) { return a < b ? a : b;}
inline f32 ffmax(f32 a, f32 b) { return a > b ? a : b;}

// do this computation in view space
float BoundingBox3D::intersectRay(const glm::vec3& ro, const glm::vec3& rd)
{
    f32 tMin, tMax;
    glm::vec3 min = Cyan::vec4ToVec3(pmin);
    glm::vec3 max = Cyan::vec4ToVec3(pmax);

    // x-min, x-max
    f32 txMin = ffmin((min.x - ro.x) / rd.x, (max.x - ro.x) / rd.x);
    f32 txMax = ffmax((min.x - ro.x) / rd.x, (max.x - ro.x) / rd.x);
    // y-min, y-max
    f32 tyMin = ffmin((min.y - ro.y) / rd.y, (max.y - ro.y) / rd.y);
    f32 tyMax = ffmax((min.y - ro.y) / rd.y, (max.y - ro.y) / rd.y);
    // z-min, z-max
    f32 tzMin = ffmin((min.z - ro.z) / rd.z, (max.z - ro.z) / rd.z);
    f32 tzMax = ffmax((min.z - ro.z) / rd.z, (max.z - ro.z) / rd.z);
    tMin = ffmax(ffmax(txMin, tyMin), tzMin);
    tMax = ffmin(ffmin(txMax, tyMax), tzMax);
    if (tMin <= tMax)
    {
        return tMin > 0.f ? tMin : tMax;
    }
    return -1.0;
}

// taken from https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
/* 
    Note(Min): It seems using glm::dot() is quite slow here; Implementing own math library should be beneficial 
*/
float Triangle::intersectRay(const glm::vec3& roObjectSpace, const glm::vec3& rdObjectSpace)
{
    const float EPSILON = 0.0000001;
    glm::vec3 edge1 = m_vertices[1] - m_vertices[0];
    glm::vec3 edge2 = m_vertices[2] - m_vertices[0];
    glm::vec3 h, s, q;
    float a,f,u,v;

    h = glm::cross(rdObjectSpace, edge2);
    // a = glm::dot(edge1, h);
    a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;
    if (fabs(a) < EPSILON)
    {
        return -1.0;
    }
    f = 1.0f / a;
    s = roObjectSpace - m_vertices[0];
    // u = f * dot(s, h);
    u = f * (s.x*h.x + s.y*h.y + s.z*h.z);
    if (u < 0.0 || u > 1.0)
    {
        return -1.0;
    }
    q = glm::cross(s, edge1);
    // v = f * dot(rdObjectSpace, q);
    v = f * (rdObjectSpace.x*q.x + rdObjectSpace.y*q.y + rdObjectSpace.z*q.z);
    if (v < 0.0 || u + v > 1.0)
    {
        return -1.0;
    }
    float t = f * glm::dot(edge2, q);
    // hit
    if (t > EPSILON)
    {
        return t;
    }
    return -1.0f;
}
