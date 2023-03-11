#include "SSGI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"
#include "RenderPass.h"

namespace Cyan
{
    std::unique_ptr<SSGI> SSGI::s_instance = nullptr;

    SSGI* SSGI::create(const glm::uvec2& inResolution)
    {
        if (s_instance == nullptr)
        {
            s_instance.reset(new SSGI(inResolution));
        }
        else
        {
            if (inResolution != s_instance->resolution)
            { 
                // for now forbidden changing resolution
                assert(0);
            }
        }
        return s_instance.get();
    }

    SSGI::HitBuffer::HitBuffer(u32 inNumLayers, const glm::uvec2& resolution)
        : numLayers(inNumLayers)
    {
        GfxTexture2DArray::Spec spec(resolution.x, resolution.y, 1, numLayers, PF_RGBA16F);
        position = GfxTexture2DArray::create(spec, Sampler2D());
        normal = GfxTexture2DArray::create(spec, Sampler2D());
        radiance = GfxTexture2DArray::create(spec, Sampler2D());
    }

    SSGI::SSGI(const glm::uvec2& inRes)
        : resolution(inRes), hitBuffer(kNumSamples, inRes)
    {
    }

    void SSGI::render(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer)
    {
        GfxTexture2D* sceneDepth = gBuffer.depth.getGfxDepthTexture2D();

        // trace
        auto renderer = Renderer::get();

        CreateVS(vs, "ScreenSpaceRayTracingVS", SHADER_SOURCE_PATH "screenspace_raytracing_v.glsl");
        CreatePS(ps, "HierarchicalSSRTPS", SHADER_SOURCE_PATH "hierarchical_ssrt_p.glsl");
        CreatePixelPipeline(pipeline, "HierarchicalSSRT", vs, ps);

        renderer->drawFullscreenQuad(
            getFramebufferSize(outAO.getGfxTexture2D()),
            [outAO, outBentNormal, outIrradiance](RenderPass& pass) {
                pass.setRenderTarget(outAO.getGfxTexture2D(), 0);
                pass.setRenderTarget(outBentNormal.getGfxTexture2D(), 1);
                pass.setRenderTarget(outIrradiance.getGfxTexture2D(), 2);
            },
            pipeline,
            [this, gBuffer, sceneDepth, HiZ, inDirectDiffuseBuffer](VertexShader* vs, PixelShader* ps) {
                ps->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                ps->setTexture("depthBuffer", sceneDepth);
                ps->setTexture("normalBuffer", gBuffer.normal.getGfxTexture2D());
                ps->setTexture("HiZ", HiZ.texture.getGfxTexture2D());
                ps->setUniform("numLevels", (i32)HiZ.texture.getGfxTexture2D()->numMips);
                ps->setUniform("kMaxNumIterations", (i32)kNumIterations);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                ps->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                ps->setTexture("directLightingBuffer", inDirectDiffuseBuffer.getGfxTexture2D());
            }
        );
    }

    void SSGI::renderEx(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer)
    {
        GfxTexture2D* sceneDepth = gBuffer.depth.getGfxDepthTexture2D();

        // trace
        auto renderer = Renderer::get();

        CreateVS(vs, "ScreenSpaceRayTracingVS", SHADER_SOURCE_PATH "screenspace_raytracing_v.glsl");
        CreatePS(ps, "HierarchicalSSRTPS", SHADER_SOURCE_PATH "hierarchical_ssrt_ex_p.glsl");
        CreatePixelPipeline(pipeline, "HierarchicalSSRT", vs, ps);

        renderer->drawFullscreenQuad(
            getFramebufferSize(outAO.getGfxTexture2D()),
            [outAO, outBentNormal, outIrradiance](RenderPass& pass) {
                pass.setRenderTarget(outAO.getGfxTexture2D(), 0);
                pass.setRenderTarget(outBentNormal.getGfxTexture2D(), 1);
                pass.setRenderTarget(outIrradiance.getGfxTexture2D(), 2);
            },
            pipeline,
            [this, gBuffer, sceneDepth, HiZ, inDirectDiffuseBuffer](VertexShader* vs, PixelShader* ps) {
                ps->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                ps->setTexture("depthBuffer", sceneDepth);
                ps->setTexture("normalBuffer", gBuffer.normal.getGfxTexture2D());
                ps->setTexture("HiZ", HiZ.texture.getGfxTexture2D());
                ps->setUniform("numLevels", (i32)HiZ.texture.getGfxTexture2D()->numMips);
                ps->setUniform("kMaxNumIterations", (i32)kNumIterations);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                ps->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                ps->setTexture("directLightingBuffer", inDirectDiffuseBuffer.getGfxTexture2D());
                ps->setUniform("numSamples", (i32)kNumSamples);

                glBindImageTexture(0, hitBuffer.position->getGpuResource(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glBindImageTexture(1, hitBuffer.normal->getGpuResource(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glBindImageTexture(2, hitBuffer.radiance->getGpuResource(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            }
        );
        // final resolve pass
        {
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSRTResolvePS", SHADER_SOURCE_PATH "ssrt_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "SSRTResolve", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outAO.getGfxTexture2D()),
                [outAO, outBentNormal, outIrradiance](RenderPass& pass) {
                    pass.setRenderTarget(outIrradiance.getGfxTexture2D(), 2);
                },
                pipeline,
                [this, gBuffer](VertexShader* vs, PixelShader* ps) {
                    ps->setTexture("depthBuffer", gBuffer.depth.getGfxDepthTexture2D());
                    ps->setTexture("normalBuffer", gBuffer.normal.getGfxTexture2D());
                    ps->setTexture("hitPositionBuffer", hitBuffer.position);
                    ps->setTexture("hitNormalBuffer", hitBuffer.normal);
                    ps->setTexture("hitRadianceBuffer", hitBuffer.radiance);
                    auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                    ps->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                    ps->setUniform("reuseKernelRadius", reuseKernelRadius);
                    ps->setUniform("numReuseSamples", numReuseSamples);
                }
            );
        }
    }
}
