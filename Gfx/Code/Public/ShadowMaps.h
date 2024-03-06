#pragma once

#include <glm/glm.hpp>

#include "Geometry.h"
#include "Shader.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "CameraViewInfo.h"
#include "SceneView.h"

namespace Cyan 
{
    class Scene;
    class SceneRender;
    class DirectionalLight;

    /**
     * Cascaded Shadow Map
     */
    class GFX_API CascadedShadowMap
    {
    public:
        struct Cascade
        {
            f32 n;
            f32 f;
            glm::vec3 worldSpaceSphereBoundCenter;
            f32 worldSpaceSphereBoundRadius;
            CameraViewInfo lightCamera;
            std::unique_ptr<GHDepthTexture> depthTexture = nullptr;
        };

        CascadedShadowMap();
        ~CascadedShadowMap();

        void render(Scene* scene, DirectionalLight* directionalLight, const CameraViewInfo& camera);
        void setShaderParameters(Pipeline* p);

        static constexpr f32 cascadeBoundries[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
        static constexpr u32 kNumCascades = 4u;

        glm::uvec2 m_resolution = glm::uvec2(4096, 4096);
        glm::vec3 m_lightDirection;
        glm::mat4 m_lightViewMatrix;
        Cascade m_cascades[kNumCascades];
        CameraViewInfo m_lastPrimaryViewCamera;

#if 0
        void debugDraw(SceneRender* render, const SceneView::State& viewState);
        void debugDrawCascadeBounds(SceneRender* render, const SceneView::State& viewState);
        void debugDrawCascadeRegion(SceneRender* render, const SceneView::State& viewState);
        void debugDrawShadowCoords(SceneRender* render, const SceneView::State& viewState);
#endif
    };
}
