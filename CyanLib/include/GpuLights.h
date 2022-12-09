#pragma once

#include "Common.h"
#include "Shader.h"

namespace Cyan {
    struct GpuDirectionalShadowMap {
        glm::mat4 lightSpaceView;
        glm::mat4 lightSpaceProjection;
        u64 depthMapHandle;
        glm::vec2 padding;
    };

    struct GpuLight {
        glm::vec4 colorAndIntensity;
    };

    struct GpuDirectionalLight : public GpuLight {
        glm::vec4 direction;
    };

    const u32 kNumShadowCascades = 4u;
    struct GpuCSMDirectionalLight : public GpuDirectionalLight {
        struct Cascade {
            f32 n;
            f32 f;
            glm::vec2 padding;
            GpuDirectionalShadowMap shadowMap;
        };

        Cascade cascades[kNumShadowCascades];
    };

    struct GpuPointLight : public GpuLight {

    };

    struct GpuSkyLight : public GpuLight {
    };
}
