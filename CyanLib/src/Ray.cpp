#include "Ray.h"

namespace Cyan
{
#if 0
    // hit need to be in object space
    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace)
    {
        auto p = hitPosObjectSpace;
        auto p0 = tri.m_vertices[0];
        auto p1 = tri.m_vertices[1];
        auto p2 = tri.m_vertices[2];

        f32 totalArea = glm::length(glm::cross(p1 - p0, p2 - p0));

        f32 w = glm::length(glm::cross(p1 - p0, p - p0)) / totalArea;
        f32 v = glm::length(glm::cross(p2 - p0, p - p0)) / totalArea;
        f32 u = 1.f - v - w;
        return glm::vec3(u, v, w);
    }

    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition)
    {
        auto worldTransformMatrix = rayHit.m_node->getWorldTransform().toMatrix();
        // convert world space hit back to object space
        glm::vec3 hitPosObjectSpace = vec4ToVec3(glm::inverse(worldTransformMatrix) * glm::vec4(hitPosition, 1.f));

        auto parent = rayHit.m_node->m_meshInstance->m_mesh;
        auto tri = parent->getTriangle(rayHit.smIndex, rayHit.triIndex);
        return computeBaryCoord(tri, hitPosObjectSpace);
    }

    // return surface normal at ray hit in world space
    glm::vec3 getSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord)
    {
        auto parent = rayHit.m_node->m_meshInstance->m_mesh;
        auto sm = parent->m_subMeshes[rayHit.smIndex];
        u32 vertexOffset = rayHit.triIndex * 3;
        glm::vec3 normal = baryCoord.x * sm->m_triangles.normals[vertexOffset]
            + baryCoord.y * sm->m_triangles.normals[vertexOffset + 1]
            + baryCoord.z * sm->m_triangles.normals[vertexOffset + 2];
        normal = glm::normalize(normal);
        // convert normal back to world space
        auto worldTransformMatrix = rayHit.m_node->getWorldTransform().toMatrix();
        normal = vec4ToVec3(glm::inverse(glm::transpose(worldTransformMatrix)) * glm::vec4(normal, 0.f));
        return glm::normalize(normal);
    }
#endif
}
