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
    class CascadedShadowMap;

    class HierarchicalZBuffer
    {
    public:
        HierarchicalZBuffer(const glm::uvec2& inputDepthResolution);
        ~HierarchicalZBuffer();

        void build(GfxDepthTexture2D* sceneDepthBuffer);
        
        std::unique_ptr<GfxTexture2D> m_depthBuffer;
        glm::uvec2 m_inputDepthResolution;
        glm::uvec2 m_powerOfTwoResolution;
    };

    class SceneRender
    {
    public:
        struct Output
        {
            Output(const glm::uvec2& inRenderResolution);

            glm::uvec2 resolution;
            std::unique_ptr<HierarchicalZBuffer> hiZ;
            std::unique_ptr<GfxDepthTexture2D> depth;
            std::unique_ptr<GfxDepthTexture2D> prevFrameDepth;
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
            std::unique_ptr<GfxTexture2D> indirectIrradiance;
            std::unique_ptr<GfxTexture2D> prevFrameIndirectIrradiance;
            std::unique_ptr<GfxTexture2D> color;
            std::unique_ptr<GfxTexture2D> resolvedColor;
            std::unique_ptr<GfxTexture2D> debugColor;
        };

        SceneRender(const glm::uvec2& renderResolution);
        ~SceneRender();

        GfxDepthTexture2D* depth() { return m_output->depth.get(); }
        HierarchicalZBuffer* hiZ() { return m_output->hiZ.get(); }
        GfxDepthTexture2D* prevFrameDepth() { return m_output->prevFrameDepth.get(); }
        GfxTexture2D* normal() { return m_output->normal.get(); }
        GfxTexture2D* albedo() { return m_output->albedo.get(); }
        GfxTexture2D* metallicRoughness() { return m_output->metallicRoughness.get(); }
        GfxTexture2D* directLighting() { return m_output->directLighting.get(); }
        GfxTexture2D* directDiffuseLighting() { return m_output->directDiffuseLighting.get(); }
        GfxTexture2D* indirectLighting() { return m_output->indirectLighting.get(); }
        GfxTexture2D* lightingOnly() { return m_output->lightingOnly.get(); }
        GfxTexture2D* ao() { return m_output->ao.get(); }
        GfxTexture2D* aoHistory() { return m_output->aoHistory.get(); }
        GfxTexture2D* indirectIrradiance() { return m_output->indirectIrradiance.get(); }
        GfxTexture2D* prevFrameIndirectIrradiance() { return m_output->prevFrameIndirectIrradiance.get(); }
        GfxTexture2D* color() { return m_output->color.get(); }
        GfxTexture2D* resolvedColor() { return m_output->resolvedColor.get(); }
        GfxTexture2D* debugColor() { return m_output->debugColor.get(); }

        std::unique_ptr<CascadedShadowMap> m_csm = nullptr;

    protected:
        std::unique_ptr<Output> m_output = nullptr;
    };
}
