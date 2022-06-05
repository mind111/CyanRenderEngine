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

    enum ShadowTechnique
    {
        kPCF_Shadow = 0,   
        kVariance_Shadow
    };

    struct BasicShadowmap
    {
        Cyan::DepthTexture* shadowmap;
    };

    struct VarianceShadowmap
    {
        Cyan::Texture2DRenderable* shadowmap;
    };

    struct ShadowCascade
    {
        f32 n;
        f32 f;
        BoundingBox3D aabb;
        // ::Line frustumLines[12];
        glm::mat4 lightProjection;
        BasicShadowmap basicShadowmap;
        VarianceShadowmap vsm;
    };

    struct NewCascadedShadowmap
    {
        static const u32 kNumCascades = 4u;
        ShadowCascade   cascades[kNumCascades];
        glm::mat4       lightView;
        ShadowTechnique technique;
        RenderTarget* depthRenderTarget = nullptr;
    };

    class RasterDirectShadowManager
    {
    public:
        RasterDirectShadowManager();
        ~RasterDirectShadowManager() { };

        void initialize();
        void finalize();
        /*
        * Create resources for a shadowmap
        */
        void initShadowmap(NewCascadedShadowmap& csm, const glm::uvec2& resolution);
        /*
        * Update cascade data
        */
        void updateShadowCascade(ShadowCascade& cascade, const Camera& camera, const glm::mat4& view);
        /*
        * Draw shadow map
        */
        void render(NewCascadedShadowmap& csm, Scene* scene, const DirectionalLight& sunLight, const std::vector<Entity*>& shadowCasters);
        void pcfShadow(NewCascadedShadowmap& csm, u32& cascadeIndex, Scene* scene, const glm::mat4& lightView, const std::vector<Entity*>& shadowCasters);
        void vsmShadow();

        Shader* m_sunShadowShader;
        Shader* m_pointShadowShader;
    };

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
            DepthTexture* shadowmap = nullptr;
        } cascades[kNumCascades];

        CascadedShadowmap(const DirectionalLight& inDirectionalLight);

    private:
        void updateCascades(const Camera& camera);
        void calcCascadeAABB(Cascade& cascade, const Camera& camera);

        glm::vec3 lightDirection;
    };
}
