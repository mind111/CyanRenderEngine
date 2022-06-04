#pragma once

#include "glm.hpp"
#include "Shader.h"
#include "RenderTarget.h"
#include "Geometry.h"

struct Camera;
struct Scene;
struct Entity;

namespace Cyan
{
    struct Renderer;
    struct DirectionalLight;

    enum ShadowTechnique
    {
        kPCF_Shadow = 0,   
        kVariance_Shadow
    };

    struct BasicShadowmap
    {
        Cyan::TextureRenderable* shadowmap;
    };

    struct VarianceShadowmap
    {
        Cyan::TextureRenderable* shadowmap;
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
        // MaterialInstance* m_sunShadowMatl;
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

        IDirectionalShadowmap()
            : quality(Quality::kHigh)
        { }

        // virtual destructor for derived
        ~IDirectionalShadowmap() { }
    };

    struct CSM : public IDirectionalShadowmap
    {
        virtual void render(const Scene& scene, Renderer& renderer) override;

        CSM(const DirectionalLight& inDirectionalLight);
    private:
        TextureRenderable* shadowmap = nullptr;
    };
}
