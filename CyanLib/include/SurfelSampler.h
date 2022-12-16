#pragma once

#include "Surfel.h"
#include "RenderableScene.h"

namespace Cyan {
    class Renderer;
    struct RenderTarget;

    struct SurfelSampler {
        SurfelSampler();
        ~SurfelSampler() { }

        void sampleRandomSurfels() { }
        void sampleFixedSizeSurfels(std::vector<Surfel>& outSurfels, const RenderableScene& inScene);
        void sampleFixedNumberSurfels(std::vector<Surfel>& outSurfels, const RenderableScene& inScene);
        void deduplicate(std::vector<Surfel>& outSurfels);
        void visualize(RenderTarget* dstRenderTarget, Renderer* renderer);

        struct Triangle {
            glm::vec3 p[3];
            glm::vec3 vn[3];
            glm::vec3 albedo;
            f32 area;
        };

        struct Tessellator {
            /** note - @min:
            * 
            */
            Tessellator(const Triangle& inTri);

            virtual void tessellate(std::vector<Surfel>& outSurfels);

            struct Sample {
                glm::vec3 p;
                glm::vec3 n;
                glm::vec3 color;
            };
            std::vector<Sample> samplePoints;
            // tessellation pixel size
            const f32 kTexelRadius = .1f;
            // 'tri' is in world space
            Triangle tri;
            // triangle face normal
            glm::vec3 tn = glm::vec3(0.f, 1.f, 0.f);
            glm::mat4 tangentToWorld;
            glm::vec3 tangentPlaneBottomLeft;
            glm::vec3 tangentPlaneTopRight;
            glm::uvec2 resolution;
            glm::vec2 size;
        };

        struct InstanceDesc
        {
            glm::mat4 transform;
            glm::vec4 color;
        };

        ShaderStorageBuffer<DynamicSsboData<InstanceDesc>> surfelSamples;
        ShaderStorageBuffer<DynamicSsboData<InstanceDesc>> instanceBuffer;
    };
}
