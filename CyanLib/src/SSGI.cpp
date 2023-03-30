#include "SSGI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"
#include "AssetImporter.h"
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
        : resolution(inRes), hitBuffer(numSamples, inRes)
    {
        Sampler2D sampler = { };
        sampler.minFilter = FM_POINT;
        sampler.magFilter = FM_POINT;
        sampler.wrapS = WM_WRAP;
        sampler.wrapT = WM_WRAP;

        auto bn16x16_0 = AssetImporter::importImageSync("BlueNoise_16x16_R_0", ASSET_PATH "textures/noise/BN_16x16_R_0.png");
        blueNoiseTextures_16x16[0] = AssetManager::createTexture2D("BlueNoise_16x16_R_0", bn16x16_0, sampler);

        auto bn16x16_1 = AssetImporter::importImageSync("BlueNoise_16x16_R_1", ASSET_PATH "textures/noise/BN_16x16_R_1.png");
        blueNoiseTextures_16x16[1] = AssetManager::createTexture2D("BlueNoise_16x16_R_1", bn16x16_1, sampler);

        auto bn16x16_2 = AssetImporter::importImageSync("BlueNoise_16x16_R_2", ASSET_PATH "textures/noise/BN_16x16_R_2.png");
        blueNoiseTextures_16x16[2] = AssetManager::createTexture2D("BlueNoise_16x16_R_2", bn16x16_2, sampler);

        auto bn16x16_3 = AssetImporter::importImageSync("BlueNoise_16x16_R_3", ASSET_PATH "textures/noise/BN_16x16_R_3.png");
        blueNoiseTextures_16x16[3] = AssetManager::createTexture2D("BlueNoise_16x16_R_3", bn16x16_3, sampler);

        auto bn16x16_4 = AssetImporter::importImageSync("BlueNoise_16x16_R_4", ASSET_PATH "textures/noise/BN_16x16_R_4.png");
        blueNoiseTextures_16x16[4] = AssetManager::createTexture2D("BlueNoise_16x16_R_4", bn16x16_4, sampler);

        auto bn16x16_5 = AssetImporter::importImageSync("BlueNoise_16x16_R_5", ASSET_PATH "textures/noise/BN_16x16_R_5.png");
        blueNoiseTextures_16x16[5] = AssetManager::createTexture2D("BlueNoise_16x16_R_5", bn16x16_5, sampler);

        auto bn16x16_6 = AssetImporter::importImageSync("BlueNoise_16x16_R_6", ASSET_PATH "textures/noise/BN_16x16_R_6.png");
        blueNoiseTextures_16x16[6] = AssetManager::createTexture2D("BlueNoise_16x16_R_6", bn16x16_6, sampler);

        auto bn16x16_7 = AssetImporter::importImageSync("BlueNoise_16x16_R_7", ASSET_PATH "textures/noise/BN_16x16_R_7.png");
        blueNoiseTextures_16x16[7] = AssetManager::createTexture2D("BlueNoise_16x16_R_7", bn16x16_7, sampler);
    }

    void SSGI::render(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIrradiance, const GBuffer& gBuffer, const RenderableScene& scene, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer)
    {
        renderAmbientOcclusionAndBentNormal(outAO, outBentNormal, gBuffer, scene);
#if 0
        GfxTexture2D* sceneDepth = gBuffer.depth.getGfxDepthTexture2D();

        // trace
        auto renderer = Renderer::get();

        CreateVS(vs, "ScreenSpaceRayTracingVS", SHADER_SOURCE_PATH "screenspace_raytracing_v.glsl");
        CreatePS(ps, "SSGITracePS", SHADER_SOURCE_PATH "ssgi_tracing_p.glsl");
        CreatePixelPipeline(pipeline, "SSGITrace", vs, ps);

        renderer->drawFullscreenQuad(
            getFramebufferSize(outAO.getGfxTexture2D()),
            [outAO, outBentNormal, outIrradiance](RenderPass& pass) {
                pass.setRenderTarget(outAO.getGfxTexture2D(), 0);
                pass.setRenderTarget(outBentNormal.getGfxTexture2D(), 1);
                pass.setRenderTarget(outIrradiance.getGfxTexture2D(), 2);
            },
            pipeline,
            [this, gBuffer, sceneDepth, HiZ, inDirectDiffuseBuffer, &scene](VertexShader* vs, PixelShader* ps) {
                ps->setShaderStorageBuffer(scene.viewBuffer.get());
                ps->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                ps->setTexture("depthBuffer", sceneDepth);
                ps->setTexture("normalBuffer", gBuffer.normal.getGfxTexture2D());
                ps->setTexture("HiZ", HiZ.texture.getGfxTexture2D());
                ps->setUniform("numLevels", (i32)HiZ.texture.getGfxTexture2D()->numMips);
                ps->setUniform("kMaxNumIterations", (i32)numIterations);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024");
                ps->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                ps->setTexture("directLightingBuffer", inDirectDiffuseBuffer.getGfxTexture2D());
                ps->setUniform("numSamples", (i32)numSamples);

                glBindImageTexture(0, hitBuffer.position->getGpuResource(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glBindImageTexture(1, hitBuffer.normal->getGpuResource(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
                glBindImageTexture(2, hitBuffer.radiance->getGpuResource(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
            }
        );

        // final resolve pass
        {
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIResolvePS", SHADER_SOURCE_PATH "ssgi_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "SSRTResolve", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outAO.getGfxTexture2D()),
                [outAO, outBentNormal, outIrradiance](RenderPass& pass) {
                    pass.setRenderTarget(outAO.getGfxTexture2D(), 0);
                    pass.setRenderTarget(outBentNormal.getGfxTexture2D(), 1);
                    pass.setRenderTarget(outIrradiance.getGfxTexture2D(), 2);
                },
                pipeline,
                [this, gBuffer, &scene](VertexShader* vs, PixelShader* ps) {
                    ps->setShaderStorageBuffer(scene.viewBuffer.get());
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
#endif
    }

    void SSGI::renderAmbientOcclusionAndBentNormal(RenderTexture2D outAO, RenderTexture2D outBentNormal, const GBuffer& gBuffer, const RenderableScene& scene) 
    {
        auto sceneDepth = gBuffer.depth.getGfxDepthTexture2D();
        auto sceneNormal = gBuffer.normal.getGfxTexture2D();
        auto renderer = Renderer::get();

        static RenderTexture2D AOHistoryBuffer("AOHistory", outAO.getGfxTexture2D()->getSpec());
        RenderTexture2D temporalPassOutput("TemporalAOOutput", outAO.getGfxTexture2D()->getSpec());
        static i32 frameCount = 0;

        // taking 1 new ao sample per pixel and does temporal filtering 
        {
            GPU_DEBUG_SCOPE(SSAOSamplePassMarker, "SSAO Sampling");

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIAOPS", SHADER_SOURCE_PATH "ssgi_ao_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIAOSample", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outAO.getGfxTexture2D()),
                [temporalPassOutput](RenderPass& pass) {
                    RenderTarget aoRenderTarget(temporalPassOutput.getGfxTexture2D(), 0, glm::vec4(1.f, 1.f, 1.f, 1.f));
                    pass.setRenderTarget(aoRenderTarget, 0);
                },
                pipeline,
                [this, gBuffer, sceneDepth, sceneNormal, &scene](VertexShader* vs, PixelShader* ps) {
                    ps->setShaderStorageBuffer(scene.viewBuffer.get());
                    ps->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                    ps->setTexture("sceneDepthTexture", sceneDepth);
                    ps->setTexture("sceneNormalTexture", sceneNormal);

                    ps->setTexture("AOHistoryBuffer", AOHistoryBuffer.getGfxTexture2D());
                    ps->setTexture("blueNoiseTextures_16x16_R[0]", blueNoiseTextures_16x16[0]->getGfxResource());
                    ps->setTexture("blueNoiseTextures_16x16_R[1]", blueNoiseTextures_16x16[1]->getGfxResource());
                    ps->setTexture("blueNoiseTextures_16x16_R[2]", blueNoiseTextures_16x16[2]->getGfxResource());
                    ps->setTexture("blueNoiseTextures_16x16_R[3]", blueNoiseTextures_16x16[3]->getGfxResource());
                    ps->setTexture("blueNoiseTextures_16x16_R[4]", blueNoiseTextures_16x16[4]->getGfxResource());
                    ps->setTexture("blueNoiseTextures_16x16_R[5]", blueNoiseTextures_16x16[5]->getGfxResource());
                    ps->setTexture("blueNoiseTextures_16x16_R[6]", blueNoiseTextures_16x16[6]->getGfxResource());
                    ps->setTexture("blueNoiseTextures_16x16_R[7]", blueNoiseTextures_16x16[7]->getGfxResource());

                    auto blueNoiseTexture_1024x1024 = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
                    ps->setTexture("blueNoiseTexture_1024x1024_RGBA", blueNoiseTexture_1024x1024->gfxTexture.get());

                    ps->setUniform("numSamples", (i32)numSamples);
                    ps->setUniform("frameCount", frameCount);
                }
            );

            renderer->blitTexture(AOHistoryBuffer.getGfxTexture2D(), temporalPassOutput.getGfxTexture2D());
        }

        if (bBilateralFiltering)
        {
            // bilateral filtering
            GPU_DEBUG_SCOPE(SSAOSpatialFilteringPassMarker, "SSAO Bilateral Filtering");

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIAOFilteringPS", SHADER_SOURCE_PATH "ssgi_ao_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIAOFiltering", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outAO.getGfxTexture2D()),
                [outAO](RenderPass& pass) {
                    RenderTarget aoRenderTarget(outAO.getGfxTexture2D(), 0, glm::vec4(1.f, 1.f, 1.f, 1.f));
                    pass.setRenderTarget(aoRenderTarget, 0);
                },
                pipeline,
                [this, outAO, temporalPassOutput, sceneDepth, &scene](VertexShader* vs, PixelShader* ps) {
                    ps->setShaderStorageBuffer(scene.viewBuffer.get());
                    ps->setTexture("sceneDepthTexture", sceneDepth);
                    ps->setTexture("aoTexture", temporalPassOutput.getGfxTexture2D());
                }
            );
        }

        frameCount++;
    }
}
