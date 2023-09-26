#pragma once

#include "Core.h"
#include "MathLibrary.h"

namespace Cyan
{
    struct CORE_API AxisAlignedBoundingBox3D
    {
        AxisAlignedBoundingBox3D();

        void bound(const AxisAlignedBoundingBox3D& aabb);
        void bound(const glm::vec3& v3);
        void bound(const glm::vec4& v4);

        glm::vec4 pmin;
        glm::vec4 pmax;
    };
}
