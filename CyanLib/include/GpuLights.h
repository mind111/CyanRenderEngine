#pragma once

#include "Common.h"
#include "Shader.h"

namespace Cyan 
{
    struct GpuLight 
    {
        glm::vec4 colorAndIntensity;
    };

    const u32 kNumShadowCascades = 4u;
    struct GpuDirectionalShadowMap 
    {
        struct Cascade 
        {
            f32 n;
            f32 f;
            u64 depthMapHandle;
            glm::mat4 lightSpaceProjection;
        };

        glm::mat4 lightSpaceView;
        Cascade cascades[kNumShadowCascades];
    };

    struct GpuDirectionalLight : public GpuLight 
    {
        glm::vec4 direction;
        GpuDirectionalShadowMap shadowMap;
    };

    struct GpuPointLight : public GpuLight 
    {

    };

    struct GpuSkyLight : public GpuLight
    {

    };
}
