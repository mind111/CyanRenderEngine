#include "SSGI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "RenderPass.h"

namespace Cyan
{
#if 0
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
        m_position = GfxTexture2DArray::create(spec, Sampler2D());
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
    #if 0
        renderHorizonBasedAOAndIndirectIrradiance(outAO, outBentNormal, outIndirectIrradiance, gBuffer, inDirectDiffuseBuffer, scene);
    #else
        renderScreenSpaceRayTracedIndirectIrradiance(outIndirectIrradiance, gBuffer, HiZ, inDirectDiffuseBuffer, scene);
    #endif
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
            [this, gBuffer, sceneDepth, HiZ, inDirectDiffuseBuffer, &scene](ProgramPipeline* p) {
                p->setShaderStorageBuffer(scene.viewBuffer.get());
                p->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                p->setTexture("depthBuffer", sceneDepth);
                p->setTexture("normalBuffer", gBuffer.normal.getGfxTexture2D());
                p->setTexture("HiZ", HiZ.texture.getGfxTexture2D());
                p->setUniform("numLevels", (i32)HiZ.texture.getGfxTexture2D()->numMips);
                p->setUniform("kMaxNumIterations", (i32)numIterations);
                auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
                p->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                p->setTexture("directLightingBuffer", inDirectDiffuseBuffer.getGfxTexture2D());
                p->setUniform("numSamples", (i32)numSamples);

                glBindImageTexture(0, hitBuffer.m_position->getGpuResource(), 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
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
                [this, gBuffer, &scene](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(scene.viewBuffer.get());
                    p->setTexture("depthBuffer", gBuffer.depth.getGfxDepthTexture2D());
                    p->setTexture("normalBuffer", gBuffer.normal.getGfxTexture2D());
                    p->setTexture("hitPositionBuffer", hitBuffer.m_position);
                    p->setTexture("hitNormalBuffer", hitBuffer.normal);
                    p->setTexture("hitRadianceBuffer", hitBuffer.radiance);
                    auto blueNoiseTexture = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
                    p->setTexture("blueNoiseTexture", blueNoiseTexture->gfxTexture.get());
                    p->setUniform("reuseKernelRadius", reuseKernelRadius);
                    p->setUniform("numReuseSamples", numReuseSamples);
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
    void SSGI::renderHorizonBasedAOAndIndirectIrradiance(RenderTexture2D outAO, RenderTexture2D outBentNormal, RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene) 
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
                [this, gBuffer, sceneDepth, sceneNormal, &scene](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(scene.viewBuffer.get());
                    p->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                    p->setTexture("sceneDepthTexture", sceneDepth);
                    p->setTexture("sceneNormalTexture", sceneNormal);

                    p->setTexture("blueNoiseTextures_16x16_R[0]", blueNoiseTextures_16x16[0]->getGfxResource());
                    p->setTexture("blueNoiseTextures_16x16_R[1]", blueNoiseTextures_16x16[1]->getGfxResource());
                    p->setTexture("blueNoiseTextures_16x16_R[2]", blueNoiseTextures_16x16[2]->getGfxResource());
                    p->setTexture("blueNoiseTextures_16x16_R[3]", blueNoiseTextures_16x16[3]->getGfxResource());
                    p->setTexture("blueNoiseTextures_16x16_R[4]", blueNoiseTextures_16x16[4]->getGfxResource());
                    p->setTexture("blueNoiseTextures_16x16_R[5]", blueNoiseTextures_16x16[5]->getGfxResource());
                    p->setTexture("blueNoiseTextures_16x16_R[6]", blueNoiseTextures_16x16[6]->getGfxResource());
                    p->setTexture("blueNoiseTextures_16x16_R[7]", blueNoiseTextures_16x16[7]->getGfxResource());

                    auto blueNoiseTexture_1024x1024 = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
                    p->setTexture("blueNoiseTexture_1024x1024_RGBA", blueNoiseTexture_1024x1024->gfxTexture.get());

                    p->setUniform("numSamples", (i32)numSamples);
                    p->setUniform("frameCount", frameCount);

                    if (frameCount > 0)
                    {
                        p->setUniform("prevFrameView", prevFrameView);
                        p->setUniform("prevFrameProjection", prevFrameProjection);

                        glBindSampler(32, depthBilinearSampler);
                        p->setUniform("prevFrameSceneDepthTexture", 32);
                        glBindTextureUnit(32, prevSceneDepth.getGfxTexture2D()->getGpuResource());

                        glBindSampler(33, SSAOHistoryBilinearSampler);
                        p->setUniform("AOHistoryBuffer", 33);
                        glBindTextureUnit(33, AOHistoryBuffer.getGfxTexture2D()->getGpuResource());

                        // p->setTexture("prevFrameSceneDepthTexture", prevSceneDepth);
                        p->setTexture("AOHistoryBuffer", AOHistoryBuffer.getGfxTexture2D());
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
                [this, gBuffer, sceneDepth, sceneNormal, inDirectDiffuseBuffer, &scene](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(scene.viewBuffer.get());
                    p->setUniform("outputSize", glm::vec2(sceneDepth->width, sceneDepth->height));
                    p->setTexture("sceneDepthTexture", sceneDepth);
                    p->setTexture("sceneNormalTexture", sceneNormal);
                    p->setTexture("diffuseRadianceBuffer", inDirectDiffuseBuffer.getGfxTexture2D());
                    auto blueNoiseTexture_1024x1024 = AssetManager::getAsset<Texture2D>("BlueNoise_1024x1024_RGBA");
                    p->setTexture("blueNoiseTexture_1024x1024_RGBA", blueNoiseTexture_1024x1024->gfxTexture.get());
                    p->setUniform("numSamples", (i32)numSamples);
                    p->setUniform("normalErrorTolerance", indirectIrradianceNormalErrTolerance);
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
                [this, outAO, AOSamplingPassOutput, sceneDepth, &scene](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(scene.viewBuffer.get());
                    p->setTexture("sceneDepthTexture", sceneDepth);
                    p->setTexture("aoTexture", AOSamplingPassOutput.getGfxTexture2D());
                }
            );
        }

        frameCount++;
    }

    void SSGI::visualize(GfxTexture2D* outColor, const GBuffer& gBuffer, const RenderableScene& scene, RenderTexture2D inDirectDiffuseBuffer)
    {
        const i32 kNumSamplesPerDir = 32;
        struct Sample
        {
            glm::vec4 m_position;
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

            ctx->setComputePipeline(pipeline, [this, inDirectDiffuseBuffer](ComputeShader* cs) {
                cs->setUniform("pixelCoord", debugPixelCoord);
                cs->setTexture("diffuseRadianceBuffer", inDirectDiffuseBuffer.getGfxTexture2D());
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

            RenderPass pass(outColor->width, outColor->height);
            pass.setRenderTarget(outColor, 0, false);
            pass.viewport = {0, 0, outColor->width, outColor->height };
            GfxPipelineState gfxPipelineState;
            gfxPipelineState.depth = DepthControl::kDisable;
            pass.gfxPipelineState = gfxPipelineState;
            pass.drawLambda = [](GfxContext* ctx) {
                CreateVS(vs, "DrawScreenSpacePointVS", SHADER_SOURCE_PATH "ssgi_draw_indirect_irradiance_samples_v.glsl");
                CreatePS(ps, "DrawScreenSpacePointPS", SHADER_SOURCE_PATH "ssgi_draw_indirect_irradiance_samples_p.glsl");
                CreatePixelPipeline(pipeline, "DrawScreenSpacePoint", vs, ps);
                ctx->setPixelPipeline(pipeline, [](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(&sampleBuffer);
                });
                u32 numVerts = cpuFrontSampleBuffer.numSamples + cpuBackSampleBuffer.numSamples;
                ctx->setVertexArray(VertexArray::getDummyVertexArray());
                glDrawArrays(GL_POINTS, 0, numVerts);
            };
            pass.render(ctx);

            // todo: draw sample directions
        }
    }

    /**
     * Screen space Hi-Z traced indirect irradiance leveraging ReSTIR
     */
    void SSGI::renderScreenSpaceRayTracedIndirectIrradiance(RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene)
    {
        GPU_DEBUG_SCOPE(IndirectIrradiancePassMarker, "SSGI Indirect Irradiance");
#if 1
        ReSTIRScreenSpaceRayTracedIndirectIrradiance(outIndirectIrradiance, gBuffer, HiZ, inDirectDiffuseBuffer, scene);
#else
        bruteforceScreenSpaceRayTracedIndirectIrradiance(outIndirectIrradiance, gBuffer, HiZ, inDirectDiffuseBuffer, scene);
#endif
    }

    void SSGI::bruteforceScreenSpaceRayTracedIndirectIrradiance(RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene)
    {
        auto sceneDepth = gBuffer.depth.getGfxDepthTexture2D();
        auto sceneNormal = gBuffer.normal.getGfxTexture2D();
        auto out = outIndirectIrradiance.getGfxTexture2D();
        auto renderer = Renderer::get();

        static i32 frameCount = 0;
        GfxTexture2D::Spec indirectIrradianceSpec(out->width, out->height, 1, PF_RGB32F);
        static RenderTexture2D temporalIndirectIrradianceBuffer[2] = { RenderTexture2D("TemporalIndirectIrradianceBuffer_0", indirectIrradianceSpec), RenderTexture2D("TemporalIndirectIrradianceBuffer_1", indirectIrradianceSpec) };

        static glm::mat4 prevFrameView(1.f);
        static glm::mat4 prevFrameProjection(1.f);

        GfxTexture2D::Spec spec(out->width, out->height, 1, PF_RGB32F);
        static RenderTexture2D prevFrameSceneDepth("PrevFrameSceneDepth", spec);

        u32 src = (u32)(frameCount - 1)% 2;
        u32 dst = (u32)frameCount % 2;

        CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
        CreatePS(ps, "SSGIBruteforceTemporal", SHADER_SOURCE_PATH "ssgi_bruteforce_temporal_p.glsl");
        CreatePixelPipeline(pipeline, "SSGIBruteforceTemporal", vs, ps);

        renderer->drawFullscreenQuad(
            getFramebufferSize(outIndirectIrradiance.getGfxTexture2D()),
            [out, src, dst](RenderPass& pass) {
                RenderTarget renderTarget(temporalIndirectIrradianceBuffer[dst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                pass.setRenderTarget(renderTarget, 0);
            },
            pipeline,
            [this, out, HiZ, inDirectDiffuseBuffer, sceneDepth, sceneNormal, &scene, src](ProgramPipeline* p) {
                p->setShaderStorageBuffer(scene.viewBuffer.get());
                p->setUniform("outputSize", glm::vec2(out->width, out->height));

                p->setTexture("HiZ", HiZ.texture.getGfxTexture2D());
                p->setUniform("numLevels", (i32)HiZ.texture.getGfxTexture2D()->numMips);
                p->setUniform("kMaxNumIterations", (i32)numIterations);

                p->setTexture("sceneDepthBuffer", sceneDepth);
                p->setTexture("sceneNormalBuffer", sceneNormal); 
                p->setTexture("diffuseRadianceBuffer", inDirectDiffuseBuffer.getGfxTexture2D());

                p->setUniform("frameCount", frameCount);
                p->setTexture("temporalIndirectIrradianceBuffer", temporalIndirectIrradianceBuffer[src].getGfxTexture2D());

                p->setUniform("prevFrameView", prevFrameView);
                p->setUniform("prevFrameProjection", prevFrameProjection);
                p->setTexture("prevFrameSceneDepthBuffer", prevFrameSceneDepth.getGfxTexture2D());
            }
        );

        renderer->blitTexture(out, temporalIndirectIrradianceBuffer[dst].getGfxTexture2D());

        // for temporal reprojection related calculations
        prevFrameView = scene.view.view;
        prevFrameProjection = scene.view.projection;
        renderer->blitTexture(prevFrameSceneDepth.getGfxTexture2D(), sceneDepth);

        frameCount++;
    }

#define APPLY_SPATIAL_REUSE 1
    void SSGI::ReSTIRScreenSpaceRayTracedIndirectIrradiance(RenderTexture2D outIndirectIrradiance, const GBuffer& gBuffer, const HiZBuffer& HiZ, RenderTexture2D inDirectDiffuseBuffer, const RenderableScene& scene)
    {
        auto sceneDepth = gBuffer.depth.getGfxDepthTexture2D();
        auto sceneNormal = gBuffer.normal.getGfxTexture2D();
        auto out = outIndirectIrradiance.getGfxTexture2D();
        auto renderer = Renderer::get();

        static i32 frameCount = 0;

        GfxTexture2D::Spec spec(out->width, out->height, 1, PF_RGB32F);
        static RenderTexture2D temporalReservoirRadiances[2] = { RenderTexture2D("TemporalReservoirRadiance_0", spec), RenderTexture2D("TemporalReservoirRadiance_1", spec) };
        static RenderTexture2D temporalReservoirSamplePositions[2] = { RenderTexture2D("TemporalReservoirSamplePosition_0", spec), RenderTexture2D("TemporalReservoirSamplePosition_1", spec) };
        static RenderTexture2D temporalReservoirSampleNormals[2] = { RenderTexture2D("TemporalReservoirSampleNormal_0", spec), RenderTexture2D("TemporalReservoirSampleNormal_1", spec) };
        static RenderTexture2D temporalReservoirWSumMW[2] = { RenderTexture2D("TemporalReservoirWSumMW_0", spec), RenderTexture2D("TemporalReservoirWSumMW_1", spec) };
        static glm::mat4 prevFrameView(1.f);
        static glm::mat4 prevFrameProjection(1.f);

        GfxTexture2D::Spec depthSpec(out->width, out->height, 1, PF_R32F);
        static RenderTexture2D prevFrameSceneDepth("PrevFrameSceneDepth", depthSpec);
        u32 temporalPassSrc = (u32)(frameCount - 1)% 2;
        u32 temporalPassDst = (u32)frameCount % 2;
        {
            GPU_DEBUG_SCOPE(temporalReSTIRMarker, "SSGI ReSTIR Temporal");

            // temporal pass
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIReSTIRTemporal", SHADER_SOURCE_PATH "ssgi_restir_temporal_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIReSTIRTemporal", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outIndirectIrradiance.getGfxTexture2D()),
                [out, temporalPassSrc, temporalPassDst](RenderPass& pass) {
                        {
                            RenderTarget renderTarget(temporalReservoirRadiances[temporalPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 0, false);
                        }
                        {
                            RenderTarget renderTarget(temporalReservoirSamplePositions[temporalPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 1, false);
                        }
                        {
                            RenderTarget renderTarget(temporalReservoirSampleNormals[temporalPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 2, false);
                        }
                        {
                            RenderTarget renderTarget(temporalReservoirWSumMW[temporalPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 3, false);
                        }
                },
                pipeline,
                [this, out, HiZ, inDirectDiffuseBuffer, sceneDepth, sceneNormal, &scene, temporalPassSrc](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(scene.viewBuffer.get());
                    p->setUniform("outputSize", glm::vec2(out->width, out->height));

                    p->setTexture("HiZ", HiZ.texture.getGfxTexture2D());
                    p->setUniform("numLevels", (i32)HiZ.texture.getGfxTexture2D()->numMips);
                    p->setUniform("kMaxNumIterations", (i32)numIterations);
                    p->setUniform("useReSTIR", bUseReSTIR ? 1.f : 0.f);

                    p->setTexture("sceneDepthBuffer", sceneDepth);
                    p->setTexture("sceneNormalBuffer", sceneNormal); 
                    p->setTexture("diffuseRadianceBuffer", inDirectDiffuseBuffer.getGfxTexture2D());

                    p->setUniform("frameCount", frameCount);
                    p->setTexture("temporalReservoirRadiance", temporalReservoirRadiances[temporalPassSrc].getGfxTexture2D());
                    p->setTexture("temporalReservoirSamplePosition", temporalReservoirSamplePositions[temporalPassSrc].getGfxTexture2D());
                    p->setTexture("temporalReservoirSampleNormal", temporalReservoirSampleNormals[temporalPassSrc].getGfxTexture2D());
                    p->setTexture("temporalReservoirWSumMW", temporalReservoirWSumMW[temporalPassSrc].getGfxTexture2D());

                    p->setUniform("prevFrameView", prevFrameView);
                    p->setUniform("prevFrameProjection", prevFrameProjection);
                    p->setTexture("prevFrameSceneDepthBuffer", prevFrameSceneDepth.getGfxTexture2D());
                }
            );

            // for temporal reprojection related calculations
            prevFrameView = scene.view.view;
            prevFrameProjection = scene.view.projection;
            renderer->blitTexture(prevFrameSceneDepth.getGfxTexture2D(), sceneDepth);
        }

        RenderTexture2D spatialReservoirRadiances[2] = { RenderTexture2D("SpatialReservoirRadiance_0", spec), RenderTexture2D("SpatialReservoirRadiance_1", spec) };
        RenderTexture2D spatialReservoirSamplePositions[2] = { RenderTexture2D("SpatialReservoirSamplePosition_0", spec), RenderTexture2D("SpatialReservoirSamplePosition_1", spec) };
        RenderTexture2D spatialReservoirSampleNormals[2] = { RenderTexture2D("SpatialReservoirSampleNormal_0", spec), RenderTexture2D("SpatialReservoirSampleNormal_1", spec) };
        RenderTexture2D spatialReservoirWSumMW[2] = { RenderTexture2D("SpatialReservoirWSumMW_0", spec), RenderTexture2D("SpatialReservoirWSumMW_1", spec) };

        {
            GPU_DEBUG_SCOPE(spatialReSTIRMarker, "SSGI ReSTIR Spatial");

            {
                // copy data from dst temporal reservoir buffers to src spatial reservoir buffers
                CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
                CreatePS(ps, "SSGIReSTIRSpatialCopy", SHADER_SOURCE_PATH "ssgi_restir_spatial_copy_p.glsl");
                CreatePixelPipeline(pipeline, "SSGIReSTIRSpatialCopy", vs, ps);

                u32 src = ((u32)0 - 1) % 2;

                renderer->drawFullscreenQuad(
                    getFramebufferSize(out),
                    [out, spatialReservoirRadiances, spatialReservoirSamplePositions, spatialReservoirSampleNormals, spatialReservoirWSumMW, src](RenderPass& pass) {
                        {
                            RenderTarget renderTarget(spatialReservoirRadiances[src].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 0, false);
                        }
                        {
                            RenderTarget renderTarget(spatialReservoirSamplePositions[src].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 1, false);
                        }
                        {
                            RenderTarget renderTarget(spatialReservoirSampleNormals[src].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 2, false);
                        }
                        {
                            RenderTarget renderTarget(spatialReservoirWSumMW[src].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 3, false);
                        }
                    },
                    pipeline,
                    [this, src](ProgramPipeline* p) {
                        p->setTexture("temporalReservoirRadiance", temporalReservoirRadiances[src].getGfxTexture2D());
                        p->setTexture("temporalReservoirSamplePosition", temporalReservoirSamplePositions[src].getGfxTexture2D());
                        p->setTexture("temporalReservoirSampleNormal", temporalReservoirSampleNormals[src].getGfxTexture2D());
                        p->setTexture("temporalReservoirWSumMW", temporalReservoirWSumMW[src].getGfxTexture2D());
                    }
                );
            }

            // iteratively apply spatial reuse
            // todo£ºjacobian calculation is still broken
            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIReSTIRSpatial", SHADER_SOURCE_PATH "ssgi_restir_spatial_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIReSTIRSpatial", vs, ps);

            for (i32 i = 0; i < kNumSpatialReuseIterations; ++i)
            {
                u32 src = ((u32)i - 1) % 2;
                i32 dst = (u32)i % 2;

                renderer->drawFullscreenQuad(
                    getFramebufferSize(out),
                    [out, spatialReservoirRadiances, spatialReservoirSamplePositions, spatialReservoirSampleNormals, spatialReservoirWSumMW, dst](RenderPass& pass) {
                        {
                            RenderTarget renderTarget(spatialReservoirRadiances[dst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 0, false);
                        }
                        {
                            RenderTarget renderTarget(spatialReservoirSamplePositions[dst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 1, false);
                        }
                        {
                            RenderTarget renderTarget(spatialReservoirSampleNormals[dst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 2, false);
                        }
                        {
                            RenderTarget renderTarget(spatialReservoirWSumMW[dst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 3, false);
                        }
                    },
                    pipeline,
                    [this, i, out, inDirectDiffuseBuffer, sceneDepth, sceneNormal, src, spatialReservoirRadiances, spatialReservoirSamplePositions, spatialReservoirSampleNormals, spatialReservoirWSumMW, &scene](ProgramPipeline* p) {
                        p->setShaderStorageBuffer(scene.viewBuffer.get());
                        p->setUniform("numSamples", numSpatialReuseSamples);
                        p->setUniform("outputSize", glm::vec2(out->width, out->height));
                        p->setTexture("sceneDepthBuffer", sceneDepth);
                        p->setTexture("sceneNormalBuffer", sceneNormal);
                        p->setUniform("iteration", i);
                        p->setUniform("reuseKernelRadius", ReSTIRSpatialReuseKernalRadius);
                        p->setUniform("frameCount", frameCount);

                        p->setTexture("spatialReservoirRadiance", spatialReservoirRadiances[src].getGfxTexture2D());
                        p->setTexture("spatialReservoirSamplePosition", spatialReservoirSamplePositions[src].getGfxTexture2D());
                        p->setTexture("spatialReservoirSampleNormal", spatialReservoirSampleNormals[src].getGfxTexture2D());
                        p->setTexture("spatialReservoirWSumMW", spatialReservoirWSumMW[src].getGfxTexture2D());
                    }
                );
            }
        }

        GfxTexture2D::Spec indirectIrradianceSpec(out->width, out->height, 1, PF_RGB16F);
        RenderTexture2D unfilteredIndirectIrradiance("SSGIUnfilteredIndirectIrradiance", indirectIrradianceSpec);
        {
            // final resolve pass
            i32 spatialPassDst = ((u32)kNumSpatialReuseIterations - 1) % 2;

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIReSTIRResolve", SHADER_SOURCE_PATH "ssgi_restir_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIReSTIRResolve", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(outIndirectIrradiance.getGfxTexture2D()),
                [unfilteredIndirectIrradiance](RenderPass& pass) {
                    RenderTarget renderTarget(unfilteredIndirectIrradiance.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                    pass.setRenderTarget(renderTarget, 0);
                },
                pipeline,
                [this, out, sceneDepth, sceneNormal, &scene, spatialPassDst, temporalPassDst, spatialReservoirRadiances, spatialReservoirSamplePositions, spatialReservoirSampleNormals, spatialReservoirWSumMW](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(scene.viewBuffer.get());
                    p->setTexture("sceneDepthBuffer", sceneDepth);
                    p->setTexture("sceneNormalBuffer", sceneNormal);
#if APPLY_SPATIAL_REUSE
                    p->setTexture("reservoirRadiance", spatialReservoirRadiances[spatialPassDst].getGfxTexture2D());
                    p->setTexture("reservoirSamplePosition", spatialReservoirSamplePositions[spatialPassDst].getGfxTexture2D());
                    p->setTexture("reservoirSampleNormal", spatialReservoirSampleNormals[spatialPassDst].getGfxTexture2D());
                    p->setTexture("reservoirWSumMW", spatialReservoirWSumMW[spatialPassDst].getGfxTexture2D());
#else
                    p->setTexture("reservoirRadiance", temporalReservoirRadiances[temporalPassDst].getGfxTexture2D());
                    p->setTexture("reservoirSamplePosition", temporalReservoirSamplePositions[temporalPassDst].getGfxTexture2D());
                    p->setTexture("reservoirSampleNormal", temporalReservoirSampleNormals[temporalPassDst].getGfxTexture2D());
                    p->setTexture("reservoirWSumMW", temporalReservoirWSumMW[temporalPassDst].getGfxTexture2D());
#endif
                }
            );
        }

        // todo: better denoiser..? 
        {
            GPU_DEBUG_SCOPE(SSGIDenoisingPass, "SSGI Denoise");

            CreateVS(vs, "BlitVS", SHADER_SOURCE_PATH "blit_v.glsl");
            CreatePS(ps, "SSGIDenoise", SHADER_SOURCE_PATH "ssgi_denoising_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIDenoise", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(out),
                [out](RenderPass& pass) {
                    RenderTarget renderTarget(out, 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                    pass.setRenderTarget(renderTarget, 0);
                },
                pipeline,
                [this, sceneDepth, unfilteredIndirectIrradiance, &scene](ProgramPipeline* p) {
                    p->setShaderStorageBuffer(scene.viewBuffer.get());
                    p->setTexture("sceneDepthBuffer", sceneDepth);
                    p->setTexture("unfilteredIndirectIrradiance", unfilteredIndirectIrradiance.getGfxTexture2D());
                }
            );
        }

        frameCount++;
    }
#endif

    SSGIRenderer::SSGIRenderer()
    {
        {
            Sampler2D sampler = { };
            sampler.minFilter = FM_POINT;
            sampler.magFilter = FM_POINT;
            sampler.wrapS = WM_WRAP;
            sampler.wrapT = WM_WRAP;

            auto bn1024x1024 = AssetImporter::importImage("BlueNoise_1024x1024_RGBA", ASSET_PATH "textures/noise/BN_1024x1024_RGBA.png");
            m_blueNoise_1024x1024_RGBA = AssetManager::createTexture2D("BlueNoise_1024x1024_RGBA", bn1024x1024, sampler);

            auto bn16x16_0 = AssetImporter::importImage("BlueNoise_16x16_R_0", ASSET_PATH "textures/noise/BN_16x16_R_0.png");
            m_blueNoise_16x16_R[0] = AssetManager::createTexture2D("BlueNoise_16x16_R_0", bn16x16_0, sampler);
            auto bn16x16_1 = AssetImporter::importImage("BlueNoise_16x16_R_1", ASSET_PATH "textures/noise/BN_16x16_R_1.png");
            m_blueNoise_16x16_R[1] = AssetManager::createTexture2D("BlueNoise_16x16_R_1", bn16x16_1, sampler);
            auto bn16x16_2 = AssetImporter::importImage("BlueNoise_16x16_R_2", ASSET_PATH "textures/noise/BN_16x16_R_2.png");
            m_blueNoise_16x16_R[2] = AssetManager::createTexture2D("BlueNoise_16x16_R_2", bn16x16_2, sampler);
            auto bn16x16_3 = AssetImporter::importImage("BlueNoise_16x16_R_3", ASSET_PATH "textures/noise/BN_16x16_R_3.png");
            m_blueNoise_16x16_R[3] = AssetManager::createTexture2D("BlueNoise_16x16_R_3", bn16x16_3, sampler);
            auto bn16x16_4 = AssetImporter::importImage("BlueNoise_16x16_R_4", ASSET_PATH "textures/noise/BN_16x16_R_4.png");
            m_blueNoise_16x16_R[4] = AssetManager::createTexture2D("BlueNoise_16x16_R_4", bn16x16_4, sampler);
            auto bn16x16_5 = AssetImporter::importImage("BlueNoise_16x16_R_5", ASSET_PATH "textures/noise/BN_16x16_R_5.png");
            m_blueNoise_16x16_R[5] = AssetManager::createTexture2D("BlueNoise_16x16_R_5", bn16x16_5, sampler);
            auto bn16x16_6 = AssetImporter::importImage("BlueNoise_16x16_R_6", ASSET_PATH "textures/noise/BN_16x16_R_6.png");
            m_blueNoise_16x16_R[6] = AssetManager::createTexture2D("BlueNoise_16x16_R_6", bn16x16_6, sampler);
            auto bn16x16_7 = AssetImporter::importImage("BlueNoise_16x16_R_7", ASSET_PATH "textures/noise/BN_16x16_R_7.png");
            m_blueNoise_16x16_R[7] = AssetManager::createTexture2D("BlueNoise_16x16_R_7", bn16x16_7, sampler);
        }
    }

    SSGIRenderer::~SSGIRenderer()
    {
    }

    void SSGIRenderer::renderAO(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        auto renderer = Renderer::get();
        auto outAO = render->ao();
        GfxTexture2D::Spec spec = outAO->getSpec();
        RenderTexture2D aoTemporalOutput("AOTemporalOutput", spec);

        // temporal pass: taking 1 new ao sample per pixel and does temporal filtering 
        {
            GPU_DEBUG_SCOPE(SSAOSampling, "SSAOSampling");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "SSGIAOPS", SHADER_SOURCE_PATH "ssgi_ao_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIAOSamplingPass", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->ao()),
                [aoTemporalOutput](RenderPass& pass) {
                    RenderTarget aoRenderTarget(aoTemporalOutput.getGfxTexture2D(), 0, glm::vec4(1.f, 1.f, 1.f, 1.f));
                    pass.setRenderTarget(aoRenderTarget, 0);
                },
                pipeline,
                [this, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);

                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("prevFrameSceneDepthBuffer", render->prevFrameDepth());
                    p->setTexture("sceneNormalBuffer", render->normal());
                    p->setTexture("AOHistoryBuffer", render->aoHistory());

                    p->setTexture("blueNoise_1024x1024_RGBA", m_blueNoise_1024x1024_RGBA->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[0]", m_blueNoise_16x16_R[0]->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[1]", m_blueNoise_16x16_R[1]->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[2]", m_blueNoise_16x16_R[2]->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[3]", m_blueNoise_16x16_R[3]->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[4]", m_blueNoise_16x16_R[4]->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[5]", m_blueNoise_16x16_R[5]->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[6]", m_blueNoise_16x16_R[6]->getGfxResource());
                    p->setTexture("blueNoise_16x16_R[7]", m_blueNoise_16x16_R[7]->getGfxResource());

                    // todo: once sampler object is properly implemented, recover the following code
#if 0
                    if (frameCount > 0)
                    {
                        p->setUniform("prevFrameViewMatrix", prevFrameView);
                        p->setUniform("prevFrameProjection", prevFrameProjection);

                        glBindSampler(32, depthBilinearSampler);
                        p->setUniform("prevFrameSceneDepthTexture", 32);
                        glBindTextureUnit(32, prevSceneDepth.getGfxTexture2D()->getGpuResource());

                        glBindSampler(33, SSAOHistoryBilinearSampler);
                        p->setUniform("AOHistoryBuffer", 33);
                        glBindTextureUnit(33, AOHistoryBuffer.getGfxTexture2D()->getGpuResource());

                        // p->setTexture("prevFrameSceneDepthTexture", prevSceneDepth);
                        p->setTexture("AOHistoryBuffer", AOHistoryBuffer.getGfxTexture2D());
                    }
#endif
                }
            );
        }

        // spatial pass that applies bilateral filtering
        {
            GPU_DEBUG_SCOPE(SSAOBilaterial, "SSAOBilateralFiltering");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "SSAOBilateralPS", SHADER_SOURCE_PATH "ssgi_ao_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIAOBilateral", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->ao()),
                [render](RenderPass& pass) {
                    RenderTarget aoRenderTarget(render->ao(), 0, glm::vec4(1.f, 1.f, 1.f, 1.f));
                    pass.setRenderTarget(aoRenderTarget, 0);
                },
                pipeline,
                [this, outAO, aoTemporalOutput, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);

                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("aoTemporalOutput", aoTemporalOutput.getGfxTexture2D());
                }
            );
        }

        // todo: could potentially feed spatial output to history buffer
        renderer->blitTexture(render->aoHistory(), aoTemporalOutput.getGfxTexture2D());
    }
}
