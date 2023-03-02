#pragma once

#include <glm/glm.hpp>

#include "Shader.h"
#include "RenderTarget.h"
#include "RenderableScene.h"
#include "Geometry.h"

namespace Cyan 
{
    struct Camera;
    struct PerspectiveCamera;
    struct OrthographicCamera;
    class Renderer;
    struct DirectionalLight;
    struct Scene;

    /**
     *  Cascaded Shadow Map implementation
     */
    struct DirectionalShadowMap
    {
        struct Cascade
        {
            // 'n' and 'f' are measured in distance from the camera
            f32 n;
            f32 f;
            BoundingBox3D worldSpaceAABB;
            std::unique_ptr<GfxDepthTexture2DBindless> depthTexture = nullptr;
            glm::mat4 lightSpaceProjection;
        };

        virtual void render(Scene* inScene, Renderer* renderer);

        DirectionalShadowMap(const DirectionalLight& inDirectionalLight);
        virtual ~DirectionalShadowMap() { }

        GpuDirectionalShadowMap buildGpuDirectionalShadowMap();

        static constexpr f32 cascadeBoundries[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
        static constexpr u32 kNumCascades = 4u;

    protected:
        glm::uvec2 resolution;
        glm::vec3 lightDirection;
    private:
        void updateCascades(const PerspectiveCamera& camera);
        Cascade cascades[kNumCascades];
    };
}
