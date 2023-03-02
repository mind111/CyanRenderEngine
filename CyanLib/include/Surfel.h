#pragma once

#include "Common.h"
#include <glm/glm.hpp>

namespace Cyan {
    struct Surfel {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 albedo;
        f32 radius;
    };

    struct GpuSurfel {
        glm::vec4 positionAndRadius;
        glm::vec4 normal;
        glm::vec4 albedo;
        glm::vec4 radiance;
    };

    struct Instance {
        glm::mat4 transform;
        glm::vec4 albedo;
        glm::vec4 radiance;
        glm::vec4 debugColor;
    };
}
