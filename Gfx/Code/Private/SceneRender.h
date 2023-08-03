#pragma once

#include "Core.h"
#include "MathLibrary.h"

namespace Cyan
{
    class Scene;
    class SceneCamera;
    class ProgramPipeline;
    class GHDepthTexture;
    class GHTexture2D;
    class CascadedShadowMap;

    class SceneRender
    {
    public:
        struct Output
        {
            Output(const glm::uvec2& inRenderResolution);

            glm::uvec2 resolution;
            std::unique_ptr<GHDepthTexture> depth;
            std::unique_ptr<GHDepthTexture> prevFrameDepth;
            std::unique_ptr<GHTexture2D> normal;
            std::unique_ptr<GHTexture2D> albedo;
            std::unique_ptr<GHTexture2D> metallicRoughness;
            std::unique_ptr<GHTexture2D> directLighting;
            std::unique_ptr<GHTexture2D> directDiffuseLighting;
            std::unique_ptr<GHTexture2D> indirectLighting;
            std::unique_ptr<GHTexture2D> lightingOnly;
            std::unique_ptr<GHTexture2D> aoHistory;
            std::unique_ptr<GHTexture2D> ao;
            std::unique_ptr<GHTexture2D> bentNormal;
            std::unique_ptr<GHTexture2D> indirectIrradiance;
            std::unique_ptr<GHTexture2D> prevFrameIndirectIrradiance;
            std::unique_ptr<GHTexture2D> color;
            std::unique_ptr<GHTexture2D> resolvedColor;
            std::unique_ptr<GHTexture2D> debugColor;
            // ReSTIR related
#if 0
            std::unique_ptr<GHTexture2D> reservoirRadiance;
            std::unique_ptr<GHTexture2D> reservoirPosition;
            std::unique_ptr<GHTexture2D> reservoirNormal;
            std::unique_ptr<GHTexture2D> reservoirWSumMW;
            std::unique_ptr<GHTexture2D> prevFrameReservoirRadiance;
            std::unique_ptr<GHTexture2D> prevFrameReservoirPosition;
            std::unique_ptr<GHTexture2D> prevFrameReservoirNormal;
            std::unique_ptr<GHTexture2D> prevFrameReservoirWSumMW;
#endif
        };

        // todo: implement this ...
        struct Extension
        {
            std::string name;
            std::unique_ptr<GHTexture2D> gfxTexture;
        };

        SceneRender(const glm::uvec2& renderResolution);
        ~SceneRender();

        GHDepthTexture* depth() { return m_output->depth.get(); }
        // HierarchicalZBuffer* hiZ() { return m_output->hiZ.get(); }
        GHDepthTexture* prevFrameDepth() { return m_output->prevFrameDepth.get(); }
        GHTexture2D* normal() { return m_output->normal.get(); }
        GHTexture2D* albedo() { return m_output->albedo.get(); }
        GHTexture2D* metallicRoughness() { return m_output->metallicRoughness.get(); }
        GHTexture2D* directLighting() { return m_output->directLighting.get(); }
        GHTexture2D* directDiffuseLighting() { return m_output->directDiffuseLighting.get(); }
        GHTexture2D* indirectLighting() { return m_output->indirectLighting.get(); }
        GHTexture2D* lightingOnly() { return m_output->lightingOnly.get(); }
        GHTexture2D* ao() { return m_output->ao.get(); }
        GHTexture2D* aoHistory() { return m_output->aoHistory.get(); }
        GHTexture2D* indirectIrradiance() { return m_output->indirectIrradiance.get(); }
        GHTexture2D* prevFrameIndirectIrradiance() { return m_output->prevFrameIndirectIrradiance.get(); }
        GHTexture2D* color() { return m_output->color.get(); }
        GHTexture2D* resolvedColor() { return m_output->resolvedColor.get(); }
        GHTexture2D* debugColor() { return m_output->debugColor.get(); }
#if 0
        GHTexture2D* reservoirRadiance() { return m_output->reservoirRadiance.get(); }
        GHTexture2D* reservoirPosition() { return m_output->reservoirPosition.get(); }
        GHTexture2D* reservoirNormal() { return m_output->reservoirNormal.get(); }
        GHTexture2D* reservoirWSumMW() { return m_output->reservoirWSumMW.get(); }
        GHTexture2D* prevFrameReservoirRadiance() { return m_output->prevFrameReservoirRadiance.get(); }
        GHTexture2D* prevFrameReservoirPosition() { return m_output->prevFrameReservoirPosition.get(); }
        GHTexture2D* prevFrameReservoirNormal() { return m_output->prevFrameReservoirNormal.get(); }
        GHTexture2D* prevFrameReservoirWSumMW() { return m_output->prevFrameReservoirWSumMW.get(); }
#endif

        std::unique_ptr<CascadedShadowMap> m_csm = nullptr;

    protected:
        std::unique_ptr<Output> m_output = nullptr;
    };
}
