#include "Color.h"

namespace Cyan
{
    glm::vec3 Color::randomColor()
    {
        f32 r = (f32)rand() / RAND_MAX;
        f32 g = (f32)rand() / RAND_MAX;
        f32 b = (f32)rand() / RAND_MAX;
        return glm::vec3(r, g, b);
    }
}