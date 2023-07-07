#include "SceneRender.h"
#include "GfxTexture.h"
#include "Camera.h"
#include "Scene.h"
#include "ShadowMaps.h"
#include "CyanRenderer.h"
#include "RenderPass.h"

namespace Cyan
{
    HierarchicalZBuffer::HierarchicalZBuffer(const glm::uvec2& inputDepthResolution)
        : m_inputDepthResolution(inputDepthResolution)
    {
        if (!isPowerOf2(inputDepthResolution.x))
        {
            u32 x = inputDepthResolution.x;
            u32 power = 0;
            while (x > 1)
            {
                x /= 2u;
                power++;
            }
            m_powerOfTwoResolution.x = pow(2, power);
        }
        else
        {
            m_powerOfTwoResolution.x = m_inputDepthResolution.x;
        }

        if (!isPowerOf2(inputDepthResolution.y))
        {
            u32 y = inputDepthResolution.y;
            u32 power = 0;
            while (y > 1)
            {
                y /= 2u;
                power++;
            }
            m_powerOfTwoResolution.y = pow(2, power);
        }
        else
        {
            m_powerOfTwoResolution.y = m_inputDepthResolution.y;
        }

        assert(isPowerOf2(m_powerOfTwoResolution.x));
        assert(isPowerOf2(m_powerOfTwoResolution.y));

        u32 numMips = log2(glm::min(m_powerOfTwoResolution.x, m_powerOfTwoResolution.y)) + 1;
        GfxTexture2D::Spec spec(m_powerOfTwoResolution.x, m_powerOfTwoResolution.y, numMips, PF_R32F);
        Sampler2D sampler = { };
        // this is necessary if want to use textureLod() to sample a mip level!!!
        sampler.minFilter = Sampler::Filtering::kMipmapPoint;
        m_depthBuffer.reset(GfxTexture2D::create(spec, sampler));
    }

    HierarchicalZBuffer::~HierarchicalZBuffer()
    {

    }

    void HierarchicalZBuffer::build(GfxDepthTexture2D* sceneDepthBuffer)
    {
        GPU_DEBUG_SCOPE(hiZBuild, "BuildHierarchicalZBuffer");

        auto renderer = Renderer::get();
        // downsample
        {
            // todo: maybe this downsampling pass needs to do more accurate things
            // like calculating the min depth of region covered by a texel in the destination
            // color buffer instead of doing simple point sampling..?
            GPU_DEBUG_SCOPE(hiZDownsample, "HierarchicalZDownsample");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "HiZDownsample", SHADER_SOURCE_PATH "hiz_downsample_p.glsl");
            CreatePixelPipeline(p, "HiZDownsample", vs, ps);

            GfxPipelineState gps = { };
            gps.depth = DepthControl::kEnable;
            renderer->drawStaticMesh(
                m_powerOfTwoResolution,
                [this](RenderPass& pass) {
                    pass.setRenderTarget(m_depthBuffer.get(), 0); // this by default binds mip 0
                },
                { 0, 0, m_powerOfTwoResolution.x, m_powerOfTwoResolution.y },
                renderer->m_quad.get(),
                p,
                [this, sceneDepthBuffer](ProgramPipeline* p) {
                    p->setTexture("sceneDepthBuffer", sceneDepthBuffer);
                    p->setUniform("outputResolution", glm::vec2(m_powerOfTwoResolution));
                },
                gps
            );
        }
        // build mipchain
        {
            GPU_DEBUG_SCOPE(hiZBuildMipChain, "HierarchicalZBuildMipChain");

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "BuildHiZPS", SHADER_SOURCE_PATH "build_hi_z_p.glsl");
            CreatePixelPipeline(pipeline, "BuildHiZ", vs, ps);
            u32 mipWidth = m_depthBuffer->width;
            u32 mipHeight = m_depthBuffer->height;
            for (i32 i = 1; i < m_depthBuffer->numMips; ++i)
            {
                mipWidth /= 2u;
                mipHeight /= 2u;
                auto src = i - 1;
                auto dst = i;
                renderer->drawFullscreenQuad(
                    glm::uvec2(mipWidth, mipHeight),
                    [this, dst](RenderPass& pass) {
                        pass.setRenderTarget(RenderTarget(m_depthBuffer.get(), dst), 0);
                    },
                    pipeline,
                    [this, src](ProgramPipeline* p) {
                        p->setUniform("srcMip", src);
                        p->setTexture("srcDepthTexture", m_depthBuffer.get());
                    }
                );
            }
        }
    }

    SceneRender::Output::Output(const glm::uvec2& renderResolution)
        : resolution(renderResolution)
    {
        // depth
        {
            GfxDepthTexture2D::Spec spec(resolution.x, resolution.y, 1);
            depth = std::unique_ptr<GfxDepthTexture2D>(GfxDepthTexture2D::create(spec, Sampler2D()));
            prevFrameDepth = std::unique_ptr<GfxDepthTexture2D>(GfxDepthTexture2D::create(spec, Sampler2D()));
            hiZ = std::make_unique<HierarchicalZBuffer>(resolution);
        }
        // normal
        {
            GfxTexture2D::Spec spec(resolution.x, resolution.y, 1, PF_RGB32F);
            normal = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            // ReSTIR related
            reservoirPosition = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            reservoirNormal = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            reservoirWSumMW = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameReservoirPosition = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameReservoirNormal = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameReservoirWSumMW = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
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
            indirectIrradiance = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameIndirectIrradiance = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            color = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            resolvedColor = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            debugColor = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            // ReSTIR related
            reservoirRadiance = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
            prevFrameReservoirRadiance = std::unique_ptr<GfxTexture2D>(GfxTexture2D::create(spec, Sampler2D()));
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