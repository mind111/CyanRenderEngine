#include "SceneRender.h"
#include "GHTexture.h"
#include "Scene.h"
#include "RenderPass.h"

namespace Cyan
{
    SceneRender::Output::Output(const glm::uvec2& renderResolution)
        : resolution(renderResolution)
    {
        // depth
        {
            GHDepthTexture::Desc desc = GHDepthTexture::Desc::create(resolution.x, resolution.y, 1);
            depth = std::move(GHDepthTexture::create(desc));
            prevFrameDepth = std::move(GHDepthTexture::create(desc));
        }
        // normal
        {
            GHTexture2D::Desc desc = GHTexture2D::Desc::create(resolution.x, resolution.y, 1, PF_RGB32F);
            normal = std::move(GHTexture2D::create(desc));
#if 0
            // ReSTIR related
            reservoirPosition = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            reservoirNormal = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            reservoirWSumMW = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameReservoirPosition = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameReservoirNormal = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameReservoirWSumMW = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
#endif
        }
        // lighting
        {
            GHTexture2D::Desc desc = GHTexture2D::Desc::create(resolution.x, resolution.y, 1, PF_RGB16F);
            albedo = std::move(GHTexture2D::create(desc));
            metallicRoughness = std::move(GHTexture2D::create(desc));
            directLighting = std::move(GHTexture2D::create(desc));
            directDiffuseLighting = std::move(GHTexture2D::create(desc));
            indirectLighting = std::move(GHTexture2D::create(desc));
            lightingOnly = std::move(GHTexture2D::create(desc));
            ao = std::move(GHTexture2D::create(desc));
            aoHistory = std::move(GHTexture2D::create(desc));
            indirectIrradiance = std::move(GHTexture2D::create(desc));
            prevFrameIndirectIrradiance = std::move(GHTexture2D::create(desc));
            color = std::move(GHTexture2D::create(desc));
            resolvedColor = std::move(GHTexture2D::create(desc));
            debugColor = std::move(GHTexture2D::create(desc));
        }
    }
 
    SceneRender::SceneRender(const glm::uvec2& renderResolution)
    {
        m_output = std::make_unique<Output>(renderResolution);
        // m_csm = std::make_unique<CascadedShadowMap>();
    }

    SceneRender::~SceneRender()
    {

    }
}
