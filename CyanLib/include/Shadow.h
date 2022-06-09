#pragma once

#include "glm.hpp"
#include "Shader.h"
#include "RenderTarget.h"
#include "Geometry.h"

struct Camera;

namespace Cyan
{
    struct Renderer;
    struct DirectionalLight;
    struct Scene;
    struct Entity;

    struct IDirectionalShadowmap
    {
        enum class Quality
        {
            kLow,
            kMedium,
            kHigh
        } quality;

        virtual void render(const Scene& scene, Renderer& renderer) { }
        virtual void setShaderParameters(Shader* shader) { }

        IDirectionalShadowmap()
            : quality(Quality::kHigh)
        { }

        // virtual destructor for derived
        ~IDirectionalShadowmap() { }
    };

    struct CascadedShadowmap : public IDirectionalShadowmap
    {
        /* IDirectionalShadowmap interface */
        virtual void render(const Scene& scene, Renderer& renderer) override;
        virtual void setShaderParameters(Shader* shader) override;

        static constexpr u32 kNumCascades = 4u;
        struct Cascade
        {
            f32 n;
            f32 f;
            BoundingBox3D aabb;
            glm::mat4 lightProjection;
            std::unique_ptr<DepthTexture> depthTexturePtr = nullptr;
        } cascades[kNumCascades];

        CascadedShadowmap(const DirectionalLight& inDirectionalLight);

    private:
        void updateCascades(const Camera& camera);
        void calcCascadeAABB(Cascade& cascade, const Camera& camera);

        glm::vec3 lightDirection;
    };
}
