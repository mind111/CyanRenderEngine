#pragma once

#include "glm.hpp"
#include "Shader.h"
#include "RenderTarget.h"
#include "RenderableScene.h"
#include "Geometry.h"

namespace Cyan
{
    struct Camera;
    struct PerspectiveCamera;
    struct OrthographicCamera;
    struct Renderer;
    struct DirectionalLight;
    struct Scene;
    struct SceneRenderable;
    struct Entity;

    struct IDirectionalShadowmap
    {
        enum class Quality
        {
            kLow,
            kMedium,
            kHigh
        } quality;

        /** note:
            todo: render() function not only will render a shadowmap, it should also render a shadowmask (a scene color texture that only renders shadow) texture 
            that can be directly sampled to determine the shadow coefficient each pixel during scene lighting pass.
        */
        /**
        * 'worldSpaceAABB` is an AABB in world space bounding the volume that need to be shadow mapped, mainly to help defining the light space projection matrix
        */
        virtual void render(ICamera* inCamera, const Scene& scene, SceneRenderable& renderableScene, Renderer& renderer) { }
        virtual void setShaderParameters(Shader* shader, const char* uniformNamePrefix) { }

        IDirectionalShadowmap(const DirectionalLight& inDirectionalLight);
        // virtual destructor for derived
        virtual ~IDirectionalShadowmap() { }

    protected:
        glm::uvec2 resolution;
        glm::vec3 lightDirection;
    };

    /**
    * Basic directional shadowmap
    * todo: min/max depth for each cascade to help shrink the orthographic projection size
    * todo: blend between cascades to alleviate artifact when transitioning between cascades
    * todo: better shadow biasing; normal bias and receiver geometry bias
    */
    struct DirectionalShadowmap : public IDirectionalShadowmap
    {
        // virtual void render(SceneRenderable& renderableScene, const BoundingBox3D& aabb, Renderer& renderer);
        virtual void render(ICamera* inCamera, const Scene& scene, SceneRenderable& renderableScene, Renderer& renderer) override;
        virtual void setShaderParameters(Shader* shader, const char* uniformNamePrefix) override;

        DirectionalShadowmap(const DirectionalLight& inDirectionalLight, const char* depthTextureNamePrefix=nullptr);

    private:
        glm::mat4 lightSpaceProjection;
        DepthTexture2D* depthTexture = nullptr;
    };

    /**
    * Variance directional shadowmap
    */
    struct VarianceShadowmap : public IDirectionalShadowmap
    {
        virtual void render(ICamera* inCamera, const Scene& scene, SceneRenderable& renderableScene, Renderer& renderer) override { }
        virtual void setShaderParameters(Shader* shader, const char* uniformNamePrefix) override { }

        VarianceShadowmap(const DirectionalLight& inDirectionalLight);

    private:
        glm::mat4 lightSpaceProjection;
        DepthTexture2D* depthTexture = nullptr;
        DepthTexture2D* depthSquaredTexture = nullptr;
    };

    struct CascadedShadowmap : public IDirectionalShadowmap
    {
        /* IDirectionalShadowmap interface */
        virtual void render(ICamera* inCamera, const Scene& scene, SceneRenderable& renderableScene, Renderer& renderer) override;
        virtual void setShaderParameters(Shader* shader, const char* uniformNamePrefix) override;

        static constexpr f32 cascadeBoundries[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
        static constexpr u32 kNumCascades = 4u;
        struct Cascade
        {
            // 'n' and 'f' are measured in distance from the camera
            f32 n;
            f32 f;
            BoundingBox3D worldSpaceAABB;
            std::unique_ptr<IDirectionalShadowmap> shadowmapPtr;
        } cascades[kNumCascades];
        CascadedShadowmap(const DirectionalLight& inDirectionalLight);

    private:
        void updateCascades(PerspectiveCamera& camera);
        void calcCascadeAABB(Cascade& cascade, PerspectiveCamera& camera);
    };
}
