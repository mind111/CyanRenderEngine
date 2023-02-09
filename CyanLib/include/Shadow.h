#pragma once

#include <glm/glm.hpp>

#include "Shader.h"
#include "RenderTarget.h"
#include "RenderableScene.h"
#include "Geometry.h"

namespace Cyan {
    struct Camera;
    struct PerspectiveCamera;
    struct OrthographicCamera;
    struct Renderer;
    struct DirectionalLight;
    struct Scene;
    struct RenderableScene;
    struct Entity;

    struct IDirectionalShadowMap
    {
        virtual void render(const BoundingBox3D& lightSpaceAABB, RenderableScene& scene, Renderer* renderer) { }
        virtual void setShaderParameters(Shader* shader, const char* uniformNamePrefix) { }

        IDirectionalShadowMap(const DirectionalLight& inDirectionalLight);
        // virtual destructor for derived
        virtual ~IDirectionalShadowMap() { 
            assert(numDirectionalShadowMaps > 0);
            numDirectionalShadowMaps--;
        }

        static u32 numDirectionalShadowMaps;
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
    struct DirectionalShadowMap : public IDirectionalShadowMap 
    {
        virtual void render(const BoundingBox3D& likghtSpaceAABB, RenderableScene& scene, Renderer* renderer) override;
        virtual void setShaderParameters(Shader* shader, const char* uniformNamePrefix) override;

        DirectionalShadowMap(const DirectionalLight& inDirectionalLight);
        ~DirectionalShadowMap() {
            IDirectionalShadowMap::~IDirectionalShadowMap();
        }

        glm::mat4 lightSpaceProjection;
        std::unique_ptr<DepthTexture2DBindless> depthTexture = nullptr;
    };

    /**
    * Variance directional shadowmap
    */
    struct VarianceShadowMap : public IDirectionalShadowMap {
        virtual void render(const BoundingBox3D& lightSpaceAABB, RenderableScene& scene, Renderer* renderer) override { }
        virtual void setShaderParameters(Shader* shader, const char* uniformNamePrefix) override { }

        VarianceShadowMap(const DirectionalLight& inDirectionalLight);
        ~VarianceShadowMap() {
            IDirectionalShadowMap::~IDirectionalShadowMap();
        }

    private:
        glm::mat4 lightSpaceProjection;
        std::unique_ptr<DepthTexture2D> depthTexture = nullptr;
        std::unique_ptr<DepthTexture2D> depthSquaredTexture = nullptr;
    };

    struct CascadedShadowMap : public IDirectionalShadowMap {
        /* IDirectionalShadowmap interface */
        virtual void render(const BoundingBox3D& lightSpaceAABB, RenderableScene& scene, Renderer* renderer) override;

        static constexpr f32 cascadeBoundries[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
        static constexpr u32 kNumCascades = 4u;
        struct Cascade
        {
            // 'n' and 'f' are measured in distance from the camera
            f32 n;
            f32 f;
            BoundingBox3D worldSpaceAABB;
            std::unique_ptr<DirectionalShadowMap> shadowMap;
        } cascades[kNumCascades];
        CascadedShadowMap(const DirectionalLight& inDirectionalLight);

    private:
        void updateCascades(const RenderableScene::Camera& camera);
        void calcCascadeAABB(Cascade& cascade, const RenderableScene::Camera& camera);
    };
}
