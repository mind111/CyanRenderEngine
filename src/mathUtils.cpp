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
}
