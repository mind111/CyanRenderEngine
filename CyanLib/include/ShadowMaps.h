#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Geometry.h"
#include "GfxTexture.h"

namespace Cyan 
{
    class Scene;
    class SceneRender;
    class DirectionalLight;
    class Camera;
    class PerspectiveCamera;
    class OrthographicCamera;

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
            BoundingBox3D worldSpaceAABB;
            glm::vec3 worldSpaceSphereBoundCenter;
            f32 worldSpaceSphereBoundRadius;
            std::unique_ptr<OrthographicCamera> camera = nullptr;
            std::unique_ptr<GfxDepthTexture2D> depthTexture = nullptr;
        };

        CascadedShadowMap(DirectionalLight* directionalLight);
        ~CascadedShadowMap();

        void render(Scene* scene, Camera* camera);

        static constexpr f32 cascadeBoundries[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
        static constexpr u32 kNumCascades = 4u;

        DirectionalLight* m_directionalLight = nullptr;
        glm::uvec2 m_resolution = glm::uvec2(4096, 4096);
        glm::vec3 m_lightDirection;
        glm::mat4 m_lightViewMatrix;
        Cascade m_cascades[kNumCascades];
        PerspectiveCamera* m_lastCameraUsedForRendering = nullptr;

    private:
        void update(PerspectiveCamera* camera);
    public:
        // debugging related stuffs
        void visualizeViewFrustum(SceneRender* render);
        void visualizeCascadeSphereBounds(SceneRender* render);
    };
}
