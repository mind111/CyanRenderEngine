#include "SceneRender.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "Scene.h"
#include "RenderPass.h"
#include "ShadowMaps.h"
#include "RenderingUtils.h"

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
        const auto& desc = GHTexture2D::Desc::create(m_powerOfTwoResolution.x, m_powerOfTwoResolution.y, numMips, PF_R32F);
        GHSampler2D sampler = { };
        sampler.setAddressingModeX(SamplerAddressingMode::Clamp);
        sampler.setAddressingModeY(SamplerAddressingMode::Clamp);
        // this is necessary if want to use textureLod() to sample a mip level!!!
        sampler.setFilteringModeMin(Sampler2DFilteringMode::PointMipmapPoint);
        sampler.setFilteringModeMag(Sampler2DFilteringMode::Bilinear);
        m_depthBuffer = std::move(GHTexture2D::create(desc, sampler));
    }

    HierarchicalZBuffer::~HierarchicalZBuffer()
    {

    }

    void HierarchicalZBuffer::build(GHDepthTexture* sceneDepthTex)
    {
        GPU_DEBUG_SCOPE(hiZBuild, "BuildHierarchicalZBuffer");

        // downsample
        {
            // todo: maybe this downsampling pass needs to do more accurate things
            // like calculating the min depth of region covered by a texel in the destination
            // color buffer instead of doing simple point sampling..?
            GPU_DEBUG_SCOPE(hiZDownsample, "HierarchicalZDownsample");

            bool bFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "HIZDownsamplePS", ENGINE_SHADER_PATH "hiz_downsample_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);

            glm::ivec2 mip0Size; 
            m_depthBuffer->getMipSize(mip0Size.x, mip0Size.y, 0);

            RenderingUtils::renderScreenPass(
                glm::uvec2(mip0Size.x, mip0Size.y),
                [this](RenderPass& pass) {
                    RenderTarget rt(m_depthBuffer.get(), 0);
                    pass.setRenderTarget(rt, 0);
                },
                gfxp.get(),
                [this, sceneDepthTex](GfxPipeline* p) {
                    p->setTexture("u_sceneDepthTex", sceneDepthTex);
                    p->setUniform("u_outputResolution", glm::vec2(m_powerOfTwoResolution));
                }
            );
        }

        // build mipchain
        {
            GPU_DEBUG_SCOPE(hiZBuildMipChain, "HierarchicalZBuildMipChain");

            bool bFound = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(bFound, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(bFound, "BuildHIZPS", ENGINE_SHADER_PATH "build_hiz_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(bFound, vs, ps);

            const auto& desc = m_depthBuffer->getDesc();
            for (i32 i = 1; i < desc.numMips; ++i)
            {
                auto src = i - 1;
                auto dst = i;

                glm::ivec2 mipSize;
                m_depthBuffer->getMipSize(mipSize.x, mipSize.y, dst);

                RenderingUtils::renderScreenPass(
                    glm::uvec2(mipSize.x, mipSize.y),
                    [this, dst](RenderPass& pass) {
                        RenderTarget rt(m_depthBuffer.get(), dst);
                        pass.setRenderTarget(rt, 0);
                    },
                    gfxp.get(),
                    [this, sceneDepthTex, src](GfxPipeline* p) {
                        p->setUniform("u_srcMip", src);
                        p->setTexture("u_srcDepthTexture", m_depthBuffer.get());
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
            GHDepthTexture::Desc desc = GHDepthTexture::Desc::create(resolution.x, resolution.y, 1);
            depth = std::move(GHDepthTexture::create(desc));
            prevFrameDepth = std::move(GHDepthTexture::create(desc));
            hiz = std::make_unique<HierarchicalZBuffer>(glm::uvec2(desc.width, desc.height));
        }
        // normal
        {
            GHTexture2D::Desc desc = GHTexture2D::Desc::create(resolution.x, resolution.y, 1, PF_RGB32F);
            normal = std::move(GHTexture2D::create(desc));

            // ReSTIR related
            reservoirSkyRadiance = std::move(GHTexture2D::create(desc));
            reservoirSkyDirection = std::move(GHTexture2D::create(desc));
            reservoirSkyWSumMW = std::move(GHTexture2D::create(desc));
            prevFrameReservoirSkyRadiance = std::move(GHTexture2D::create(desc));
            prevFrameReservoirSkyDirection = std::move(GHTexture2D::create(desc));
            prevFrameReservoirSkyWSumMW = std::move(GHTexture2D::create(desc));

            reservoirRadiance = std::move(GHTexture2D::create(desc));
            reservoirPosition = std::move(GHTexture2D::create(desc));
            reservoirNormal = std::move(GHTexture2D::create(desc));
            reservoirWSumMW = std::move(GHTexture2D::create(desc));
            prevFrameReservoirRadiance = std::move(GHTexture2D::create(desc));
            prevFrameReservoirPosition = std::move(GHTexture2D::create(desc));
            prevFrameReservoirNormal = std::move(GHTexture2D::create(desc));
            prevFrameReservoirWSumMW = std::move(GHTexture2D::create(desc));
        }
        // lighting
        {
            GHTexture2D::Desc desc = GHTexture2D::Desc::create(resolution.x, resolution.y, 1, PF_RGB16F);
            albedo = std::move(GHTexture2D::create(desc));
            metallicRoughness = std::move(GHTexture2D::create(desc));
            directLighting = std::move(GHTexture2D::create(desc));
            directDiffuseLighting = std::move(GHTexture2D::create(desc));
            directSkyIrradiance = std::move(GHTexture2D::create(desc));
            prevFrameDirectSkyIrradiance = std::move(GHTexture2D::create(desc));
            directSkyReflection = std::move(GHTexture2D::create(desc));
            prevFrameDirectSkyReflection = std::move(GHTexture2D::create(desc));
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
        m_csm = std::make_unique<CascadedShadowMap>();
    }

    SceneRender::~SceneRender()
    {

    }
}
