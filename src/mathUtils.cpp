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
}
