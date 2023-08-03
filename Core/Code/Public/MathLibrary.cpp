#include "MathLibrary.h"

namespace Cyan
{
    glm::vec3 vec4ToVec3(const glm::vec4& v4)
    {
        return glm::vec3(v4.x, v4.y, v4.z);
    }
}