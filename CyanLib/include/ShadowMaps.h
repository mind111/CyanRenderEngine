#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Geometry.h"
#include "GfxTexture.h"

namespace Cyan 
{
    class Scene;
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
            std::unique_ptr<OrthographicCamera> camera = nullptr;
            std::unique_ptr<GfxDepthTexture2D> depthTexture = nullptr;
        };

        CascadedShadowMap(DirectionalLight* directionalLight);
        ~CascadedShadowMap();

        void update(PerspectiveCamera* camera);
        void render(Scene* scene, Camera* camera);

        static constexpr f32 cascadeBoundries[5] = { 0.0f, 0.1f, 0.3f, 0.6f, 1.0f };
        static constexpr u32 kNumCascades = 4u;

        DirectionalLight* m_directionalLight = nullptr;
        glm::uvec2 m_resolution = glm::uvec2(4096, 4096);
        glm::vec3 m_lightDirection;
        glm::mat4 m_lightViewMatrix;
        Cascade m_cascades[kNumCascades];
    };
}
