#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Common.h"

namespace Cyan
{
    struct GfxDepthTexture2D;
    struct GfxTexture2D;
    class Scene;
    class Camera;
    class ProgramPipeline;

    class SceneRender
    {
    public:

        struct Output
        {
            Output(const glm::uvec2& inRenderResolution);

            glm::uvec2 resolution;
            std::unique_ptr<GfxDepthTexture2D> HiZ;
            std::unique_ptr<GfxDepthTexture2D> depth;
            std::unique_ptr<GfxTexture2D> normal;
            std::unique_ptr<GfxTexture2D> albedo;
            std::unique_ptr<GfxTexture2D> metallicRoughness;
            std::unique_ptr<GfxTexture2D> directLighting;
            std::unique_ptr<GfxTexture2D> directDiffuseLighting;
            std::unique_ptr<GfxTexture2D> indirectLighting;
            std::unique_ptr<GfxTexture2D> lightingOnly;
            std::unique_ptr<GfxTexture2D> aoHistory;
            std::unique_ptr<GfxTexture2D> ao;
            std::unique_ptr<GfxTexture2D> bentNormal;
            std::unique_ptr<GfxTexture2D> irradiance;
            std::unique_ptr<GfxTexture2D> color;
        };

        struct ViewParameters
        {
            ViewParameters(Scene* scene, Camera* camera);

            void setShaderParameters(ProgramPipeline* p) const;

            glm::uvec2 renderResolution;
            f32 aspectRatio;
            glm::mat4 viewMatrix;
            glm::mat4 projectionMatrix;
            glm::vec3 cameraPosition;
            glm::vec3 cameraLookAt;
            glm::vec3 cameraRight;
            glm::vec3 cameraForward;
            glm::vec3 cameraUp;
            i32 frameCount;
            f32 elapsedTime;
            f32 deltaTime;
        };

        SceneRender(Scene* scene, Camera* camera);
        ~SceneRender() { }

        void update();

        GfxDepthTexture2D* depth() { return m_output->depth.get(); }
        GfxTexture2D* normal() { return m_output->normal.get(); }
        GfxTexture2D* albedo() { return m_output->albedo.get(); }
        GfxTexture2D* metallicRoughness() { return m_output->metallicRoughness.get(); }
        GfxTexture2D* directLighting() { return m_output->directLighting.get(); }
        GfxTexture2D* directDiffuseLighting() { return m_output->directDiffuseLighting.get(); }
        GfxTexture2D* indirectLighting() { return m_output->indirectLighting.get(); }
        GfxTexture2D* lightingOnly() { return m_output->lightingOnly.get(); }
        GfxTexture2D* ao() { return m_output->ao.get(); }
        GfxTexture2D* color() { return m_output->color.get(); }

        Scene* m_scene = nullptr;
        Camera* m_camera = nullptr;
        ViewParameters m_viewParameters;

    protected:
        std::unique_ptr<Output> m_output = nullptr;
    };
}
