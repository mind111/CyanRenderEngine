#include "SceneRender.h"
#include "GfxTexture.h"
#include "Camera.h"
#include "Scene.h"
#include "ShadowMaps.h"

namespace Cyan
{
    SceneRender::Output::Output(const glm::uvec2& renderResolution)
        : resolution(renderResolution)
    {
        // depth
        {
            GfxDepthTexture2D::Spec spec(resolution.x, resolution.y, 1);
            depth = std::unique_ptr<GfxDepthTexture2D>(GfxDepthTexture2D::create(spec, Sampler2D()));
            prevFrameDepth = std::unique_ptr<GfxDepthTexture2D>(GfxDepthTexture2D::create(spec, Sampler2D()));
        }
        // normal
        {
            GfxTexture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB32F);
            normal = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
        }
        // lighting
        {
            GfxTexture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB16F);
            albedo = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            metallicRoughness = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            directLighting = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            directDiffuseLighting = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            indirectLighting = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            lightingOnly = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            ao = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            aoHistory = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            color = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            resolvedColor = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            debugColor = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
        }
    }
 
    SceneRender::SceneRender(const glm::uvec2& renderResolution)
    {
        m_output = std::make_unique<Output>(renderResolution);
        m_csm = std::make_unique<CascadedShadowMap>();
    }

    SceneRender::~SceneRender()
    {

    }
}