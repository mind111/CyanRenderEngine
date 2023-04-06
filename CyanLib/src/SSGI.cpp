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

        glCreateSamplers(1, &depthBilinearSampler);
        glSamplerParameteri(depthBilinearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(depthBilinearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(depthBilinearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glSamplerParameteri(depthBilinearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);

        glCreateSamplers(1, &SSAOHistoryBilinearSampler);
        glSamplerParameteri(SSAOHistoryBilinearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(SSAOHistoryBilinearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(SSAOHistoryBilinearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glSamplerParameteri(SSAOHistoryBilinearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }

    void SSGI::render(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, const RenderableScene& scene, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer)
    {
#if 1
        renderAOAndIndirectIrradiance(outAO, outBentNormal, outIndirectIrradiance, gBuffer, inDirectDiffuseBuffer, scene);
        visualize(outIndirectIrradiance, gBuffer, scene);
#else
        GfxTexture2D* sceneDepth = gBuffer.depth.getGfxDepthTexture2D();

        // trace
        auto renderer = Renderer::get();

        CreateVS(vs, "ScreenSpaceRayTracingVS", SHADER_SOURCE_PATH "screenspace_raytracing_v.glsl");
        CreatePS(ps, "SSGITracePS", SHADER_SOURCE_PATH "ssgi_tracing_p.glsl");
        CreatePixelPipeline(pipeline, "SSGITrace", vs, ps);

        renderer->drawFullscreenQuad(
            getFramebufferSize(outAO.getGfxTexture2D()),
            [outAO, outBentNormal, outIndirectIrradiance](RenderPass& pass) {
                pass.setRenderTarget(outAO.getGfxTexture2D(), 0);
                pass.setRenderTarget(outBentNormal.getGfxTexture2D(), 1);
                pass.setRenderTarget(outIndirectIrradiance.getGfxTexture2D(), 2);
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
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
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
                [outAO, outBentNormal, outIndirectIrradiance](RenderPass& pass) {
                    pass.setRenderTarget(outAO.getGfxTexture2D(), 0);
                    pass.setRenderTarget(outBentNormal.getGfxTexture2D(), 1);
                    pass.setRenderTarget(outIndirectIrradiance.getGfxTexture2D(), 2);
                },
                pipeline,
                [this, gBuffer, &scene](VertexShader* vs, PixelShader* ps) {
                    ps->setShaderStorageBuffer(scene.viewBuffer.get());
                    ps->setTexture("depthBuffer", gBuffer.depth.getGfxDepthTexture2D());
                    ps->setTexture("normalBuffer", gBuffer.normal.getGfxTexture2D());
                    ps->setTexture("hitPositionBuffer", hitBuffer.position);
                    ps->setTexture("hitNormalBuffer", hitBuffer.normal);
                    ps->setTexture("hitRadianceBuffer", hitBuffer.radiance);
                    auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
                    ps->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                    ps->setUniform("reuseKernelRadius", reuseKernelRadius);
                    ps->setUniform("numReuseSamples", numReuseSamples);
                }
            );
        }
#endif
    }

    // todo: take pixel velocity into consideration when doing reprojection
    // todo: consider changes in pixel neighborhood when reusing cache sample, changes in neighborhood pixels means SSAO value can change even there is a cache hit
    // todo: adaptive convergence-aware spatial filtering (helps smooth out low sample count pixels under motion)
    // todo: the image quality still has room for improvements
    /**
     * References:
        - Practical Realtime Strategies for Accurate Indirect Occlusion https://iryoku.com/downloads/Practical-Realtime-Strategies-for-Accurate-Indirect-Occlusion.pdf
        - A Spatial and Temporal Coherence Framework for Real-Time Graphics 
        - High-Quality Screen-Space Ambient Occlusion using Temporal Coherence https://publik.tuwien.ac.at/files/PubDat_191582.pdf
     */
    void SSGI::renderAOAndIndirectIrradiance(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene) 
    {
        static i32 frameCount = 0;
        static glm::mat4 prevFrameView(1.f);
        static glm::mat4 prevFrameProjection(1.f);
        GfxTexture2D::Spec depthSpec(gBuffer.depth.getGfxDepthTexture2D()->width, gBuffer.depth.getGfxDepthTexture2D()->height, 1, PF_R32F);
        static RenderTexture2D prevSceneDepth("PrevFrameSceneDepthTexture", depthSpec);
        static RenderTexture2D AOHistoryBuffer("AOHistory", outAO.getGfxTexture2D()->getSpec());

        auto sceneDepth = gBuffer.depth.getGfxDepthTexture2D();
        auto sceneNormal = gBuffer.normal.getGfxTexture2D();
        auto renderer = Renderer::get();

        auto outAOGfxTexture = outAO.getGfxTexture2D();
        glm::uvec2 outputSize(outAOGfxTexture->width, outAOGfxTexture->height);
        GfxTexture2D::Spec aoSpec(outputSize.x, outputSize.y, 1, PF_RGBA16F);

        RenderTexture2D AOSamplingPassOutput("AOSamplingPassOutput", aoSpec);

        // taking 1 new ao sample per pixel and does temporal filtering 
        {
            GPU_DEBUG_SCOPE(SSAOSamplePassMarker, "SSAO Sampling");

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIAOPS", SHADER_SOURCE_PATH "ssgi_ao_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIAOSample", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outAO.getGfxTexture2D()),
                [AOSamplingPassOutput](RenderPass& pass) {
                    RenderTarget aoRenderTarget(AOSamplingPassOutput.getGfxTexture2D(), 0, glm::vec4(1.f, 1.f, 1.f, 1.f));
                    pass.setRenderTarget(aoRenderTarget, 0);
                },
                pipeline,
                [this, gBuffer, sceneDepth, sceneNormal, &scene](VertexShader* vs, PixelShader* ps) {
                    ps->setShaderStorageBuffer(scene.viewBuffer.get());
                    ps->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                    ps->setTexture("sceneDepthTexture", sceneDepth);
                    ps->setTexture("sceneNormalTexture", sceneNormal);

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

                    if (frameCount > 0)
                    {
                        ps->setUniform("prevFrameView", prevFrameView);
                        ps->setUniform("prevFrameProjection", prevFrameProjection);

                        glBindSampler(32, depthBilinearSampler);
                        ps->setUniform("prevFrameSceneDepthTexture", 32);
                        glBindTextureUnit(32, prevSceneDepth.getGfxTexture2D()->getGpuResource());

                        glBindSampler(33, SSAOHistoryBilinearSampler);
                        ps->setUniform("AOHistoryBuffer", 33);
                        glBindTextureUnit(33, AOHistoryBuffer.getGfxTexture2D()->getGpuResource());

                        // ps->setTexture("prevFrameSceneDepthTexture", prevSceneDepth);
                        ps->setTexture("AOHistoryBuffer", AOHistoryBuffer.getGfxTexture2D());
                    }
                }
            );

            prevFrameView = scene.view.view;
            prevFrameProjection = scene.view.projection;
            renderer->blitTexture(AOHistoryBuffer.getGfxTexture2D(), AOSamplingPassOutput.getGfxTexture2D());
            renderer->blitTexture(prevSceneDepth.getGfxTexture2D(), sceneDepth);
        }

        // indirect irradiance pass using horizon based approach
        {
            GPU_DEBUG_SCOPE(IndirectIrradiancePassMarker, "SSGI Indirect Irradiance");

            GfxTexture2D::Spec indirectIrradianceSpec(outputSize.x, outputSize.y, 1, PF_RGB16F);

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIIndirectIrradiancePS", SHADER_SOURCE_PATH "ssgi_indirect_irradiance_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIIndirectIrradianceSample", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outIndirectIrradiance.getGfxTexture2D()),
                [outIndirectIrradiance](RenderPass& pass) {
                    RenderTarget indirectIrradianceRenderTarget(outIndirectIrradiance.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                    pass.setRenderTarget(indirectIrradianceRenderTarget, 0);
                },
                pipeline,
                [this, gBuffer, sceneDepth, sceneNormal, inDirectDiffuseBuffer, &scene](VertexShader* vs, PixelShader* ps) {
                    ps->setShaderStorageBuffer(scene.viewBuffer.get());
                    ps->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                    ps->setTexture("sceneDepthTexture", sceneDepth);
                    ps->setTexture("sceneNormalTexture", sceneNormal);
                    ps->setTexture("diffuseRadianceBuffer", inDirectDiffuseBuffer.getGfxTexture2D());
                    auto blueNoiseTexture_1024x1024 = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
                    ps->setTexture("blueNoiseTexture_1024x1024_RGBA", blueNoiseTexture_1024x1024->gfxTexture.get());
                    ps->setUniform("numSamples", (i32)numSamples);
                    ps->setUniform("normalErrorTolerance", indirectIrradianceNormalErrTolerance);
                }
            );
        }

        // bilateral filtering
        {
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
                [this, outAO, AOSamplingPassOutput, sceneDepth, &scene](VertexShader* vs, PixelShader* ps) {
                    ps->setShaderStorageBuffer(scene.viewBuffer.get());
                    ps->setTexture("sceneDepthTexture", sceneDepth);
                    ps->setTexture("aoTexture", AOSamplingPassOutput.getGfxTexture2D());
                }
            );
        }

        frameCount++;
    }

    void SSGI::visualize(RenderTexture2D outColor, const GBuffer& gBuffer, const RenderableScene& scene)
    {
        const i32 kNumSamplesPerDir = 32;
        struct Sample
        {
            glm::vec4 position;
            glm::vec4 radiance;
        };
        struct SampleBuffer
        {
            u32 getSizeInBytes()
            {
                return sizeof(numSamples) + sizeof(padding) + sizeOfVector(samples);
            }

            i32 numSamples = 0;
            glm::vec3 padding = glm::vec3(0.f);
            std::vector<Sample> samples = std::vector<Sample>(kNumSamplesPerDir);
        };

        static SampleBuffer cpuFrontSampleBuffer;
        static SampleBuffer cpuBackSampleBuffer;

        static ShaderStorageBuffer gpuFrontSampleBuffer("FrontSampleBuffer", cpuFrontSampleBuffer.getSizeInBytes());
        static ShaderStorageBuffer gpuBackSampleBuffer("BackSampleBuffer", cpuBackSampleBuffer.getSizeInBytes());

        auto ctx = GfxContext::get();

        // sample pass
        {
            CreateCS(cs, "SSGIVisualizeSamplePointsCS", SHADER_SOURCE_PATH "ssgi_visualize_c.glsl");
            CreateComputePipeline(pipeline, "SSGIVisualizeSamplePoints", cs);

            ctx->setComputePipeline(pipeline, [](ComputeShader* cs) {
                cs->setUniform("pixelCoord", glm::vec2(.5f));
                cs->setUniform("sampleSliceIndex", (i32)0);
                cs->setShaderStorageBuffer(&gpuFrontSampleBuffer);
                cs->setShaderStorageBuffer(&gpuBackSampleBuffer);
            });
            glDispatchCompute(1, 1, 1);

            // read back data
            gpuFrontSampleBuffer.read(cpuFrontSampleBuffer.numSamples, 0);
            gpuFrontSampleBuffer.read(cpuFrontSampleBuffer.samples, sizeof(cpuFrontSampleBuffer.numSamples) + sizeof(cpuFrontSampleBuffer.padding), sizeOfVector(cpuFrontSampleBuffer.samples));

            gpuBackSampleBuffer.read(cpuBackSampleBuffer.numSamples, 0);
            gpuBackSampleBuffer.read(cpuBackSampleBuffer.samples, sizeof(cpuBackSampleBuffer.numSamples) + sizeof(cpuBackSampleBuffer.padding), sizeOfVector(cpuBackSampleBuffer.samples));
        }

        {
            // todo: draw sample points
            u32 totalNumSamplesToDraw = cpuFrontSampleBuffer.numSamples + cpuBackSampleBuffer.numSamples;
            static ShaderStorageBuffer sampleBuffer("SampleBuffer", sizeof(Sample) * totalNumSamplesToDraw);

            u32 offset = 0;
            sampleBuffer.write(cpuFrontSampleBuffer.samples, offset, sizeof(Sample) * cpuFrontSampleBuffer.numSamples);
            offset += sizeof(Sample) * cpuFrontSampleBuffer.numSamples;
            sampleBuffer.write(cpuBackSampleBuffer.samples, offset, sizeof(Sample) * cpuBackSampleBuffer.numSamples);

            auto outGfxTexture = outColor.getGfxTexture2D();
            RenderPass pass(outGfxTexture->width, outGfxTexture->height);
            pass.setRenderTarget(outGfxTexture, 0, false);
            pass.viewport = {0, 0, outGfxTexture->width, outGfxTexture->height };
            GfxPipelineState gfxPipelineState;
            gfxPipelineState.depth = DepthControl::kDisable;
            pass.gfxPipelineState = gfxPipelineState;
            pass.drawLambda = [](GfxContext* ctx) {
                CreateVS(vs, "DrawScreenSpacePointVS", SHADER_SOURCE_PATH "ssgi_draw_indirect_irradiance_samples_v.glsl");
                CreatePS(ps, "DrawScreenSpacePointPS", SHADER_SOURCE_PATH "ssgi_draw_indirect_irradiance_samples_p.glsl");
                CreatePixelPipeline(pipeline, "DrawScreenSpacePoint", vs, ps);
                ctx->setPixelPipeline(pipeline, [](VertexShader* vs, PixelShader* ps) {
                    vs->setShaderStorageBuffer(&sampleBuffer);
                });
                u32 numVerts = cpuFrontSampleBuffer.numSamples + cpuBackSampleBuffer.numSamples;
                glDrawArrays(GL_POINTS, 0, numVerts);
            };
            pass.render(ctx);

            // todo: draw sample directions
        }
    }
}
