#include "Bounds.h"

namespace Cyan
{
    AxisAlignedBoundingBox3D::AxisAlignedBoundingBox3D()
    {
        pmin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
        pmax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
    }

    void AxisAlignedBoundingBox3D::bound(const AxisAlignedBoundingBox3D& aabb)
    {
        pmin.x = std::min(pmin.x, aabb.pmin.x);
        pmin.y = std::min(pmin.y, aabb.pmin.y);
        pmin.z = std::min(pmin.z, aabb.pmin.z);

        pmax.x = std::max(pmax.x, aabb.pmax.x);
        pmax.y = std::max(pmax.y, aabb.pmax.y);
        pmax.z = std::max(pmax.z, aabb.pmax.z);
    }

    void AxisAlignedBoundingBox3D::bound(const glm::vec3& v3)
    {
        pmin.x = std::min(pmin.x, v3.x);
        pmin.y = std::min(pmin.y, v3.y);
        pmin.z = std::min(pmin.z, v3.z);

        pmax.x = std::max(pmax.x, v3.x);
        pmax.y = std::max(pmax.y, v3.y);
        pmax.z = std::max(pmax.z, v3.z);
    }

    void AxisAlignedBoundingBox3D::bound(const glm::vec4& v4)
    {
        pmin.x = std::min(pmin.x, v4.x);
        pmin.y = std::min(pmin.y, v4.y);
        pmin.z = std::min(pmin.z, v4.z);

        pmax.x = std::max(pmax.x, v4.x);
        pmax.y = std::max(pmax.y, v4.y);
        pmax.z = std::max(pmax.z, v4.z);
    }
}
