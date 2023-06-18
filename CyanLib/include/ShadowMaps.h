#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Geometry.h"
#include "GfxTexture.h"
#include "Camera.h"
#include "SceneCamera.h"

namespace Cyan 
{
    class Scene;
    class SceneRender;
    class DirectionalLight;

    /**
     * Cascaded Shadow Map
     */
    class CascadedShadowMap
    {
    public:
        struct Cascade
        {
            f32 n;
            f32 f;
            glm::vec3 worldSpaceSphereBoundCenter;
            f32 worldSpaceSphereBoundRadius;
            Camera lightCamera;
            std::unique_ptr<GfxDepthTexture2D> depthTexture = nullptr;
        };

        CascadedShadowMap();
        ~CascadedShadowMap();

        void render(Scene* scene, DirectionalLight* directionalLight, const Camera& camera);
        void setShaderParameters(ProgramPipeline* p);

        static constexpr f32 cascadeBoundries[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
        static constexpr u32 kNumCascades = 4u;

        glm::uvec2 m_resolution = glm::uvec2(4096, 4096);
        glm::vec3 m_lightDirection;
        glm::mat4 m_lightViewMatrix;
        Cascade m_cascades[kNumCascades];
        Camera m_lastPrimaryViewCamera;

        void debugDraw(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        void debugDrawCascadeBounds(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        void debugDrawCascadeRegion(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
        void debugDrawShadowCoords(SceneRender* render, const SceneCamera::ViewParameters& viewParameters);
    };
}
