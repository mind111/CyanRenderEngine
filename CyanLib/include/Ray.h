#pragma once

#include "Entity.h"

namespace Cyan
{
    glm::vec3 computeBaryCoord(Triangle& tri, glm::vec3& hitPosObjectSpace);
    glm::vec3 computeBaryCoordFromHit(RayCastInfo rayHit, glm::vec3& hitPosition);
    glm::vec3 getSurfaceNormal(RayCastInfo& rayHit, glm::vec3& baryCoord);
}
