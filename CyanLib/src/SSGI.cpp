#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "SSGI.h"
#include "CyanRenderer.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "RenderPass.h"
#include "ShaderStorageBuffer.h"
#include "Lights.h"

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

        m_debugger = std::make_unique<SSGIDebugger>(this);
    }

    SSGIRenderer::~SSGIRenderer()
    {
    }

    void SSGIRenderer::renderSceneIndirectLighting(Scene* scene, SceneRender * render, const SceneCamera::ViewParameters & viewParameters)
    {
        renderAO(render, viewParameters);
        renderDiffuseGI(scene->m_skyLight, render, viewParameters);

        // compose final indirect lighting
        {
            GPU_DEBUG_SCOPE(SSGIComposeIndirectLighting, "SSGIComposeIndirectLighting");

            auto renderer = Renderer::get();
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "SSGIIndirectLightingPS", SHADER_SOURCE_PATH "SSGI_indirect_lighting_p.glsl");
            CreatePixelPipeline(pipeline, "SSGIIndirectLighting", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectLighting()),
                [render](RenderPass& pass) {
                    pass.setRenderTarget(render->indirectLighting(), 0, /*bClear=*/false);
                },
                pipeline,
                [this, scene, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);
                    p->setTexture("sceneDepth", render->depth());
                    p->setTexture("sceneNormal", render->normal());
                    p->setTexture("sceneAlbedo", render->albedo());
                    p->setTexture("sceneMetallicRoughness", render->metallicRoughness());

                    p->setUniform("settings.bNearFieldSSAO", m_settings.bNearFieldSSAO ? 1.f : 0.f);
                    p->setTexture("SSGI_NearFieldSSAO", render->ao());
                    p->setUniform("settings.bIndirectIrradiance", m_settings.bIndirectIrradiance ? 1.f : 0.f);
                    p->setTexture("SSGI_Diffuse", render->indirectIrradiance());
                    p->setUniform("settings.indirectBoost", m_settings.indirectBoost);
                }
            );
        }
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

    void SSGIRenderer::renderDiffuseGI(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(SSGIDiffuse, "SSGIDiffuse");

        auto renderer = Renderer::get();

        CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
        CreatePS(ps, "SSGIStochasticTemporal", SHADER_SOURCE_PATH "ssgi_bruteforce_temporal_p.glsl");
        CreatePixelPipeline(p, "SSGIBruteforceTemporal", vs, ps);

        // temporal pass
        {
            auto outIndirectIrradiance = render->indirectIrradiance();
            RenderTexture2D temporalIndirectIrradianceBuffer("SSGITemporalIndirectIrradianceBuffer", outIndirectIrradiance->getSpec());

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectIrradiance()),
                [render](RenderPass& pass) {
                    RenderTarget renderTarget(render->indirectIrradiance(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                    pass.setRenderTarget(renderTarget, 0);
                },
                p,
                [this, skyLight, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);

                    auto hiZ = render->hiZ();
                    p->setTexture("hiz.depthQuadtree", hiZ->m_depthBuffer.get());
                    p->setUniform("hiz.numMipLevels", (i32)hiZ->m_depthBuffer->numMips);
                    p->setUniform("settings.kTraceStopMipLevel", (i32)m_settings.kTracingStopMipLevel);
                    p->setUniform("settings.kMaxNumIterationsPerRay", (i32)m_settings.kMaxNumIterationsPerRay);
                    p->setUniform("settings.bSSDO", m_settings.bSSDO ? 1.f : 0.f);

                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("sceneNormalBuffer", render->normal()); 
                    p->setTexture("diffuseRadianceBuffer", render->directDiffuseLighting());
                    p->setTexture("skyCubemap", skyLight->m_cubemap.get());

                    p->setTexture("prevFrameSceneDepthBuffer", render->prevFrameDepth());
                    p->setTexture("prevFrameIndirectIrradianceBuffer", render->prevFrameIndirectIrradiance());
                }
            );
        }
        // spatial pass
        {
        }
    }

    void SSGIRenderer::renderReflection(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
    }

    void SSGIRenderer::debugDraw(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        if (bDebugging)
        {
            m_debugger->render(render, viewParameters);
        }
    }

    void SSGIRenderer::renderUI()
    {
        if (ImGui::TreeNode("SSGI"))
        {
            ImGui::Text("SSDO"); ImGui::SameLine();
            ImGui::Checkbox("##SSDO", &m_settings.bSSDO);
            ImGui::SliderFloat("##Indirect Boost", &m_settings.indirectBoost, Settings::kMinIndirectBoost, Settings::kMaxIndirectBoost);
            ImGui::Text("Debugging"); ImGui::SameLine();
            ImGui::Checkbox("##Debugging", &bDebugging); 
            if (bDebugging)
            {
                m_debugger->renderUI();
            }
            ImGui::TreePop();
        }
    }

    SSGIDebugger::SSGIDebugger(SSGIRenderer * renderer)
        : m_SSGIRenderer(renderer)
    {
        m_rayMarchingInfos.resize(m_SSGIRenderer->m_settings.kMaxNumIterationsPerRay);
        for (i32 iter = 0; iter < m_SSGIRenderer->m_settings.kMaxNumIterationsPerRay; ++iter)
        {
            m_rayMarchingInfos[iter] = RayMarchingInfo{ };
        }
    }

    SSGIDebugger::~SSGIDebugger()
    {

    }

    void SSGIDebugger::render(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(SSGIDebugRender, "DebugDrawSSGIRay")

        debugDrawRay(render, viewParameters);
    }

    void SSGIDebugger::renderUI()
    {
        if (ImGui::TreeNode("SSGI Debugger"))
        {
            ImGui::Text("Freeze Debug Ray"); ImGui::SameLine();
            ImGui::Checkbox("##Freeze Debug Ray", &m_freezeDebugRay);

            if (!m_freezeDebugRay)
            {
                f32 debugRayScreenCoord[2] = { m_debugRayScreenCoord.x, m_debugRayScreenCoord.y };
                ImGui::Text("Debug Ray ScreenCoord"); ImGui::SameLine();
                ImGui::SliderFloat2("##Debug Ray ScreenCoord", debugRayScreenCoord, 0.f, 1.f);
                m_debugRayScreenCoord.x = debugRayScreenCoord[0];
                m_debugRayScreenCoord.y = debugRayScreenCoord[1];

                f32 depthSampleCoord[2] = { m_depthSampleCoord.x, m_depthSampleCoord.y };
                ImGui::Text("Debug DepthSample Coord"); ImGui::SameLine();
                ImGui::SliderFloat2("##Debug DepthSample Coord", depthSampleCoord, 0.f, 1.f);
                m_depthSampleCoord.x = depthSampleCoord[0];
                m_depthSampleCoord.y = depthSampleCoord[1];

                ImGui::Text("Debug DepthSample Mip"); ImGui::SameLine();
                ImGui::SliderInt("##Debug DepthSample Mip", &m_depthSampleLevel, 0, 10);
                ImGui::Text("Scene Depth: %.5f", m_depthSample.sceneDepth.x);
                ImGui::Text("Quadtree MinDepth: %.5f", m_depthSample.quadtreeMinDepth.x);
            }
            ImGui::TreePop();
        }
    }

    void drawQuadtreeCell(GfxTexture2D* outColor, GfxDepthTexture2D* depth, HierarchicalZBuffer* hiz, const SceneCamera::ViewParameters& viewParameters, i32 mipLevel, glm::vec2 cellCenter, f32 cellSize)
    {
        CreateVS(vs, "DebugDrawQuadtreeCellVS", SHADER_SOURCE_PATH "debug_draw_quadtree_cell_v.glsl");
        CreatePS(ps, "DebugDrawPS", SHADER_SOURCE_PATH "debug_draw_p.glsl");
        CreatePixelPipeline(p, "DebugDrawQuadtreeCell", vs, ps);

        RenderPass pass(outColor->width, outColor->height);
        pass.viewport = { 0, 0, outColor->width, outColor->height };
        pass.setRenderTarget(outColor, 0, /*bClear=*/false);
        pass.setDepthBuffer(depth, /*bClearDepth=*/false);
        pass.drawLambda = [=](GfxContext* ctx) {
            p->bind(ctx);
            auto va = VertexArray::getDummyVertexArray(); // work around not using a vertex array for drawing
            va->bind();
            viewParameters.setShaderParameters(p);
            p->setTexture("depthQuadtree", hiz->m_depthBuffer.get());
            p->setUniform("mipLevel", mipLevel);
            p->setUniform("cellCenter", cellCenter);
            p->setUniform("cellSize", cellSize);
            p->setUniform("cellColor", vec4ToVec3(Color::blue));
            p->setUniform("cameraView", viewParameters.viewMatrix);
            p->setUniform("cameraProjection", viewParameters.projectionMatrix);
            glDrawArrays(GL_POINTS, 0, 1);
            va->unbind();
            p->unbind(ctx);
        };
        auto renderer = Renderer::get();
        pass.render(renderer->getGfxCtx());
    }

    void SSGIDebugger::debugDrawRay(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(debugDrawSSGIRay, "DebugDrawSSGIRay")

        const auto& SSGISettings = m_SSGIRenderer->m_settings;

        // generate a ray data
        if (!m_freezeDebugRay)
        {
            if (m_debugRayBuffer == nullptr)
            {
                m_debugRayBuffer = std::make_unique<ShaderStorageBuffer>("DebugRayBuffer", sizeof(DebugRay));
            }
            if (m_rayMarchingInfoBuffer == nullptr)
            {
                m_rayMarchingInfoBuffer = std::make_unique<ShaderStorageBuffer>("RayMarchingInfoBuffer", sizeOfVector(m_rayMarchingInfos));
            }

            CreateCS(cs, "SSGIDebugGenerateRayCS", SHADER_SOURCE_PATH "ssgi_debug_ray_gen_c.glsl");
            CreateComputePipeline(p, "SSGIDebugGenerateRay", cs);

            // clear data
            for (i32 i = 0; i < SSGISettings.kMaxNumIterationsPerRay; ++i)
            {
                m_rayMarchingInfos[i] = RayMarchingInfo{ };
            }
            m_rayMarchingInfoBuffer->write(m_rayMarchingInfos, 0);

            auto ctx = GfxContext::get();
            p->bind(ctx);
            viewParameters.setShaderParameters(p);
            p->setShaderStorageBuffer(m_debugRayBuffer.get());
            p->setShaderStorageBuffer(m_rayMarchingInfoBuffer.get());

            auto hiZ = render->hiZ();
            p->setTexture("hiz.depthQuadtree", hiZ->m_depthBuffer.get());
            p->setUniform("hiz.numMipLevels", (i32)hiZ->m_depthBuffer->numMips);
            p->setUniform("settings.kTraceStopMipLevel", (i32)SSGISettings.kTracingStopMipLevel);
            p->setUniform("settings.kMaxNumIterationsPerRay", (i32)SSGISettings.kMaxNumIterationsPerRay);

            p->setTexture("sceneDepthBuffer", render->depth());
            p->setTexture("sceneNormalBuffer", render->normal());
            p->setTexture("diffuseRadianceBuffer", render->directDiffuseLighting());
            p->setUniform("debugRayScreenCoord", m_debugRayScreenCoord);
            glDispatchCompute(1, 1, 1);
            p->unbind(ctx);

            m_debugRayBuffer->read<DebugRay>(m_debugRay, 0);
            m_rayMarchingInfoBuffer->read<RayMarchingInfo>(m_rayMarchingInfos, 0, sizeOfVector(m_rayMarchingInfos));
        }

        {
            auto renderer = Renderer::get();

            // validate hiz buffer first mip
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "HizValidationPS", SHADER_SOURCE_PATH "validate_hiz_p.glsl");
            CreatePixelPipeline(p, "HizValidation", vs, ps);
            RenderTexture2D validationOutput("HizValidation", render->albedo()->getSpec());
            renderer->drawFullscreenQuad(
                getFramebufferSize(render->depth()),
                [render, validationOutput](RenderPass& pass) {
                    pass.setRenderTarget(validationOutput.getGfxTexture2D(), 0);
                },
                p,
                [render](ProgramPipeline* p) {
                    auto hiz = render->hiZ();
                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("depthQuadtree", hiz->m_depthBuffer.get());
                }
            );

            // copy scene rendering output into debug color
            renderer->blitTexture(render->debugColor(), render->resolvedColor());

            // draw screen space ray origin and screen space ray origin with offset
            {
#if 0
                // draw a screen space grid to help visualize a quadtree level
                CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
                CreatePS(ps, "DrawQuadtreeGridPS", SHADER_SOURCE_PATH "draw_quadtree_grid_p.glsl");
                CreatePixelPipeline(p, "DrawQuadtreeGrid", vs, ps);

                RenderTexture2D outColor("DrawQuadtreeGridOutput", render->debugColor()->getSpec());
                renderer->drawFullscreenQuad(
                    getFramebufferSize(render->debugColor()),
                    [outColor](RenderPass& pass) {
                        pass.setRenderTarget(outColor.getGfxTexture2D(), 0, false);
                    },
                    p,
                    [render](ProgramPipeline* p) {
                        auto hiz = render->hiZ();
                        p->setTexture("srcQuadtreeTexture", hiz->m_depthBuffer.get());
                        p->setTexture("backgroundTexture", render->debugColor());
                        p->setUniform("mip", 7);
                    }
                );
                renderer->blitTexture(render->debugColor(), outColor.getGfxTexture2D());
#endif

                std::vector<Renderer::DebugPrimitiveVertex> points(2);
                points[0].position = m_debugRay.screenSpaceRo;
                points[0].color = Color::yellow;
                points[1].position = m_debugRay.screenSpaceRoWithOffset;
                points[1].color = Color::purple;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }
            // draw ray
            if (m_debugRay.hitResult.x == f32(HitResult::kHit))
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = m_debugRay.hitRadiance;
                vertices[1].position = m_debugRay.worldSpaceHitPosition;
                vertices[1].color = m_debugRay.hitRadiance;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);

                // draw a point for the screen space hit position
                std::vector<Renderer::DebugPrimitiveVertex> points(1);
                points[0].position = m_debugRay.screenSpaceHitPosition;
                points[0].color = Color::green;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }
            else if (m_debugRay.hitResult.x == f32(HitResult::kBackface))
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = Color::purple;
                vertices[1].position = m_debugRay.worldSpaceHitPosition;
                vertices[1].color = Color::purple;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);

                // draw a point for the screen space hit position
                std::vector<Renderer::DebugPrimitiveVertex> points(1);
                points[0].position = m_debugRay.screenSpaceHitPosition;
                points[0].color = Color::purple;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }
            else if (m_debugRay.hitResult.x == f32(HitResult::kHidden))
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = Color::cyan;
                vertices[1].position = m_debugRay.worldSpaceHitPosition;
                vertices[1].color = Color::cyan;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);

                // draw a point for the screen space hit position
                std::vector<Renderer::DebugPrimitiveVertex> points(1);
                points[0].position = m_debugRay.screenSpaceHitPosition;
                points[0].color = Color::cyan;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
            }

            // draw intermediate steps
            {
                std::vector<Renderer::DebugPrimitiveVertex> screenSpacePoints;
                std::vector<Renderer::DebugPrimitiveVertex> screenSpaceDepthSamples;
                std::vector<Renderer::DebugPrimitiveVertex> worldSpacePoints;
                for (i32 iter = 0; iter < SSGISettings.kMaxNumIterationsPerRay; ++iter)
                {
                    const auto& info = m_rayMarchingInfos[iter];
                    if (info.screenSpacePosition.w > 0.f)
                    {
                        Renderer::DebugPrimitiveVertex v0 = { };
                        v0.position = info.screenSpacePosition; 
                        v0.color = Color::blue;
                        screenSpacePoints.push_back(v0);

                        Renderer::DebugPrimitiveVertex v1 = { };
                        v1.position = info.depthSampleCoord; 

                        if (info.screenSpacePosition.z < info.minDepth.x)
                        {
                            v1.color = Color::cyan;
                        }
                        else
                        {
                            v1.color = Color::orange;
                        }
                        screenSpaceDepthSamples.push_back(v1);
                    } 
                    if (info.worldSpacePosition.w > 0.f)
                    {
                        Renderer::DebugPrimitiveVertex v = { };
                        v.position = info.worldSpacePosition;
                        if (info.screenSpacePosition.z < info.minDepth.x)
                        {
                            v.color = Color::pink;
                        }
                        else
                        {
                            v.color = Color::red;
                        }
                        worldSpacePoints.push_back(v);
                    }
                }

                // renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, screenSpacePoints);
                // renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, screenSpaceDepthSamples);
                renderer->debugDrawWorldSpacePoints(render->debugColor(), render->depth(), viewParameters, worldSpacePoints);
            }

            // sample min depth
            {
                const char* src = R"(
#version 450 core
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
struct DepthSample
{
    vec4 sceneDepth;
    vec4 quadtreeDepth;
};
uniform int depthSampleLevel;
uniform vec2 depthSampleCoord;
uniform sampler2D sceneDepthBuffer;
uniform sampler2D depthQuadtree;
layout (std430) buffer DepthSampleBuffer 
{
    DepthSample depthSample;
};
void main() 
{
    float sceneDepth = texture(sceneDepthBuffer, depthSampleCoord).r;
    float quadtreeDepth = textureLod(depthQuadtree, depthSampleCoord, depthSampleLevel).r;
    depthSample.sceneDepth = vec4(vec3(sceneDepth), 1.f);
    depthSample.quadtreeDepth = vec4(vec3(quadtreeDepth), 1.f);
}
                )";
                CreateCSInline(cs, "SSGIDebugMinDepthCS", src);
                CreateComputePipeline(p, "SSGIDebugMinDepth", cs);

                if (m_depthSampleBuffer == nullptr)
                {
                    m_depthSampleBuffer = std::make_unique<ShaderStorageBuffer>("DepthSampleBuffer", sizeof(DepthSample));
                }
                auto ctx = GfxContext::get();
                p->bind(ctx);
                p->setShaderStorageBuffer(m_depthSampleBuffer.get());
                auto hiZ = render->hiZ();
                p->setTexture("depthQuadtree", hiZ->m_depthBuffer.get());
                p->setTexture("sceneDepthBuffer", render->depth());
                p->setUniform("depthSampleCoord", m_depthSampleCoord);
                p->setUniform("depthSampleLevel", m_depthSampleLevel);
                glDispatchCompute(1, 1, 1);
                p->unbind(ctx);
                // read back
                m_depthSampleBuffer->read<DepthSample>(m_depthSample, 0);
                // draw a point on screen to indicate where the depth sample is at
                std::vector<Renderer::DebugPrimitiveVertex> vertices(1);
                vertices[0].position = glm::vec4(m_depthSampleCoord * 2.f - 1.f, 0.f, 1.f);
                vertices[0].color = Color::pink;
                renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, vertices);
            }

            // draw ray start normal
            {
                std::vector<Renderer::DebugPrimitiveVertex> vertices(2);
                vertices[0].position = m_debugRay.ro;
                vertices[0].color = Color::blue;
                vertices[1].position = glm::vec4(vec4ToVec3(m_debugRay.ro) + vec4ToVec3(m_debugRay.n), 1.f);
                vertices[1].color = Color::blue;
                renderer->debugDrawWorldSpaceLines(render->debugColor(), render->depth(), viewParameters, vertices);
            }
            // draw ray hit normal
        }
    }

    void SSGIDebugger::debugDrawRayOriginAndOffset(SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        auto renderer = Renderer::get();

        // copy scene rendering output into debug color
        renderer->blitTexture(render->debugColor(), render->resolvedColor());

        // draw screen space ray origin and screen space ray origin with offset
        {
            // draw a screen space grid to help visualize a quadtree level
            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "DrawQuadtreeGridPS", SHADER_SOURCE_PATH "draw_quadtree_grid_p.glsl");
            CreatePixelPipeline(p, "DrawQuadtreeGrid", vs, ps);

            RenderTexture2D outColor("DrawQuadtreeGridOutput", render->debugColor()->getSpec());
            renderer->drawFullscreenQuad(
                getFramebufferSize(render->debugColor()),
                [outColor](RenderPass& pass) {
                    pass.setRenderTarget(outColor.getGfxTexture2D(), 0, false);
                },
                p,
                [render](ProgramPipeline* p) {
                    auto hiz = render->hiZ();
                    p->setTexture("srcQuadtreeTexture", hiz->m_depthBuffer.get());
                    p->setTexture("backgroundTexture", render->debugColor());
                    p->setUniform("mip", 5);
                }
            );
            renderer->blitTexture(render->debugColor(), outColor.getGfxTexture2D());

            std::vector<Renderer::DebugPrimitiveVertex> points(2);
            points[0].position = m_debugRay.screenSpaceRo;
            points[0].color = Color::yellow;
            points[1].position = m_debugRay.screenSpaceRoWithOffset;
            points[1].color = Color::purple;
            renderer->debugDrawScreenSpacePoints(render->debugColor(), render->depth(), viewParameters, points);
        }
    }

    ReSTIRSSGIRenderer::ReSTIRSSGIRenderer()
    {

    }

    ReSTIRSSGIRenderer::~ReSTIRSSGIRenderer()
    {

    }

    // todo: implement this
    struct Reservoir
    {
        Reservoir(const char* reservoirName);

        const char* name = nullptr;
        RenderTexture2D radiance;
        RenderTexture2D position;
        RenderTexture2D normal;
        RenderTexture2D wSumMW;
    };

    void ReSTIRSSGIRenderer::renderDiffuseGI(SkyLight* skyLight, SceneRender* render, const SceneCamera::ViewParameters& viewParameters)
    {
        GPU_DEBUG_SCOPE(ReSTIRSSGIDiffuse, "ReSTIRSSGIDiffuse");

        auto renderer = Renderer::get();

        RenderTexture2D temporalReservoirRadiance("TemporalReservoirRadiance", render->reservoirRadiance());
        RenderTexture2D temporalReservoirPosition("TemporalReservoirPosition", render->reservoirPosition());
        RenderTexture2D temporalReservoirNormal("TemporalReservoirNormal", render->reservoirNormal());
        RenderTexture2D temporalReservoirWSumMW("TemporalReservoirWSumMW", render->reservoirWSumMW());

        // temporal pass
        {
            GPU_DEBUG_SCOPE(ReSTIRSSGITemporal, "ReSTIRSSGITemporal");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "ReSTIRSSGITemporal", SHADER_SOURCE_PATH "ReSTIRSSGI_temporal_p.glsl");
            CreatePixelPipeline(pipeline, "ReSTIRSSGITemporal", vs, ps);

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectIrradiance()),
                [render, temporalReservoirRadiance, temporalReservoirPosition, temporalReservoirNormal, temporalReservoirWSumMW](RenderPass& pass) {
                    {
                        RenderTarget renderTarget(temporalReservoirRadiance.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 0);
                    }
                    {
                        RenderTarget renderTarget(temporalReservoirPosition.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 1);
                    }
                    {
                        RenderTarget renderTarget(temporalReservoirNormal.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 2);
                    }
                    {
                        RenderTarget renderTarget(temporalReservoirWSumMW.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                        pass.setRenderTarget(renderTarget, 3);
                    }
                },
                pipeline,
                [this, skyLight, render, viewParameters](ProgramPipeline* p) {
                    viewParameters.setShaderParameters(p);

                    auto hiZ = render->hiZ();
                    p->setTexture("hiz.depthQuadtree", hiZ->m_depthBuffer.get());
                    p->setUniform("hiz.numMipLevels", (i32)hiZ->m_depthBuffer->numMips);
                    p->setUniform("settings.kTraceStopMipLevel", (i32)m_settings.kTracingStopMipLevel);
                    p->setUniform("settings.kMaxNumIterationsPerRay", (i32)m_settings.kMaxNumIterationsPerRay);

                    p->setTexture("sceneDepthBuffer", render->depth());
                    p->setTexture("sceneNormalBuffer", render->normal()); 
                    p->setTexture("diffuseRadianceBuffer", render->directDiffuseLighting());
                    p->setTexture("prevFrameIndirectIrradianceBuffer", render->prevFrameIndirectIrradiance());

                    p->setTexture("prevFrameReservoirRadiance", render->prevFrameReservoirRadiance());
                    p->setTexture("prevFrameReservoirPosition", render->prevFrameReservoirPosition());
                    p->setTexture("prevFrameReservoirNormal", render->prevFrameReservoirNormal());
                    p->setTexture("prevFrameReservoirWSumMW", render->prevFrameReservoirWSumMW());
                    p->setTexture("prevFrameSceneDepthBuffer", render->prevFrameDepth());

                    p->setUniform("bTemporalResampling", bTemporalResampling ? 1.f : 0.f);
                }
            );

            // for temporal reprojection related calculations
            renderer->blitTexture(render->prevFrameReservoirRadiance(), temporalReservoirRadiance.getGfxTexture2D());
            renderer->blitTexture(render->prevFrameReservoirPosition(), temporalReservoirPosition.getGfxTexture2D());
            renderer->blitTexture(render->prevFrameReservoirNormal(), temporalReservoirNormal.getGfxTexture2D());
            renderer->blitTexture(render->prevFrameReservoirWSumMW(), temporalReservoirWSumMW.getGfxTexture2D());
        }

        i32 spatialPassSrc, spatialPassDst;
        RenderTexture2D spatialReservoirRadiances[2] = { RenderTexture2D("SpatialReservoirRadiance_0", render->reservoirRadiance()),  RenderTexture2D("SpatialReservoirRadiance_1", render->reservoirRadiance()),};
        RenderTexture2D spatialReservoirPositions[2] = { RenderTexture2D("SpatialReservoirPosition_0", render->reservoirPosition()),  RenderTexture2D("SpatialReservoirPosition_1", render->reservoirPosition()),};
        RenderTexture2D spatialReservoirNormals[2] = { RenderTexture2D("SpatialReservoirNormal_0", render->reservoirNormal()),  RenderTexture2D("SpatialReservoirNormal_1", render->reservoirNormal()),};
        RenderTexture2D spatialReservoirWSumMWs[2] = { RenderTexture2D("SpatialReservoirWSumMW_0", render->reservoirWSumMW()),  RenderTexture2D("SpatialReservoirWSumMW_1", render->reservoirWSumMW()),};

        // spatial pass
        {
            if (bSpatialResampling)
            {
                GPU_DEBUG_SCOPE(ReSTIRSSGISpatial, "ReSTIRSSGISpatial");

                CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
                CreatePS(ps, "ReSTIRSSGISpatial", SHADER_SOURCE_PATH "ReSTIRSSGI_spatial_p.glsl");
                CreatePixelPipeline(pipeline, "ReSTIRSSGISpatial", vs, ps);

                for (i32 pass = 0; pass < m_numSpatialReusePass; ++pass)
                {
                    spatialPassSrc = pass % 2;
                    spatialPassDst = (pass + 1) % 2;

                    if (pass == 0)
                    {
                        renderer->blitTexture(spatialReservoirRadiances[spatialPassSrc].getGfxTexture2D(), temporalReservoirRadiance.getGfxTexture2D());
                        renderer->blitTexture(spatialReservoirPositions[spatialPassSrc].getGfxTexture2D(), temporalReservoirPosition.getGfxTexture2D());
                        renderer->blitTexture(spatialReservoirNormals[spatialPassSrc].getGfxTexture2D(), temporalReservoirNormal.getGfxTexture2D());
                        renderer->blitTexture(spatialReservoirWSumMWs[spatialPassSrc].getGfxTexture2D(), temporalReservoirWSumMW.getGfxTexture2D());
                    }

                    renderer->drawFullscreenQuad(
                        getFramebufferSize(render->indirectIrradiance()),
                        [render, spatialPassSrc, spatialPassDst, spatialReservoirRadiances, spatialReservoirPositions, spatialReservoirNormals, spatialReservoirWSumMWs](RenderPass& pass) {
                            {
                                RenderTarget renderTarget(spatialReservoirRadiances[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 0);
                            }
                            {
                                RenderTarget renderTarget(spatialReservoirPositions[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 1);
                            }
                            {
                                RenderTarget renderTarget(spatialReservoirNormals[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 2);
                            }
                            {
                                RenderTarget renderTarget(spatialReservoirWSumMWs[spatialPassDst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                                pass.setRenderTarget(renderTarget, 3);
                            }
                        },
                        pipeline,
                        [this, render, pass, spatialPassSrc, spatialPassDst, spatialReservoirRadiances, spatialReservoirPositions, spatialReservoirNormals, spatialReservoirWSumMWs, viewParameters](ProgramPipeline* p) {
                            viewParameters.setShaderParameters(p);

                            p->setUniform("reusePass", pass);
                            p->setUniform("reuseSampleCount", m_spatialReuseSampleCount);
                            p->setUniform("reuseRadius", m_spatialReuseRadius);

                            p->setTexture("sceneDepthBuffer", render->depth());
                            p->setTexture("sceneNormalBuffer", render->normal());
                            p->setTexture("inReservoirRadiance", spatialReservoirRadiances[spatialPassSrc].getGfxTexture2D());
                            p->setTexture("inReservoirPosition", spatialReservoirPositions[spatialPassSrc].getGfxTexture2D());
                            p->setTexture("inReservoirNormal", spatialReservoirNormals[spatialPassSrc].getGfxTexture2D());
                            p->setTexture("inReservoirWSumMW", spatialReservoirWSumMWs[spatialPassSrc].getGfxTexture2D());
                            p->setUniform("bUseJacobian", bUseJacobian ? 1.f : 0.f);
                        }
                    );
                }
            }
        }

        // resolve pass
        RenderTexture2D resolved("ReSTIRSSGIResolvedIrradiance", render->indirectIrradiance());
        {
            GPU_DEBUG_SCOPE(ReSTIRSSGIResolve, "ReSTIRSSGIResolve");

            CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
            CreatePS(ps, "ReSTIRSSGIResolve", SHADER_SOURCE_PATH "ReSTIRSSGI_resolve_p.glsl");
            CreatePixelPipeline(pipeline, "ReSTIRSSGIResolve", vs, ps);

            ShaderSetupFunc withSpatialReuseShaderFunc = [this, render, viewParameters, spatialPassDst, spatialReservoirRadiances, spatialReservoirPositions, spatialReservoirNormals, spatialReservoirWSumMWs](ProgramPipeline* p) {
                viewParameters.setShaderParameters(p);
                p->setTexture("sceneDepthBuffer", render->depth());
                p->setTexture("sceneNormalBuffer", render->normal()); 
                p->setTexture("reservoirRadiance", spatialReservoirRadiances[spatialPassDst].getGfxTexture2D());
                p->setTexture("reservoirPosition", spatialReservoirPositions[spatialPassDst].getGfxTexture2D());
                p->setTexture("reservoirNormal", spatialReservoirNormals[spatialPassDst].getGfxTexture2D());
                p->setTexture("reservoirWSumMW", spatialReservoirWSumMWs[spatialPassDst].getGfxTexture2D());
            };

            ShaderSetupFunc withoutSpatialReuseShaderFunc = [this, render, viewParameters, temporalReservoirRadiance, temporalReservoirPosition, temporalReservoirNormal, temporalReservoirWSumMW](ProgramPipeline* p) {
                viewParameters.setShaderParameters(p);
                p->setTexture("sceneDepthBuffer", render->depth());
                p->setTexture("sceneNormalBuffer", render->normal()); 
                p->setTexture("reservoirRadiance", temporalReservoirRadiance.getGfxTexture2D());
                p->setTexture("reservoirPosition", temporalReservoirPosition.getGfxTexture2D());
                p->setTexture("reservoirNormal", temporalReservoirNormal.getGfxTexture2D());
                p->setTexture("reservoirWSumMW", temporalReservoirWSumMW.getGfxTexture2D());
            };

            renderer->drawFullscreenQuad(
                getFramebufferSize(render->indirectIrradiance()),
                [render, resolved](RenderPass& pass) {
                    RenderTarget renderTarget(resolved.getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                    pass.setRenderTarget(renderTarget, 0);
                },
                pipeline,
                bSpatialResampling ? withSpatialReuseShaderFunc : withoutSpatialReuseShaderFunc
            );
        }

        // denoising pass
        {
            if (bDenoisingPass)
            {
                // simple bilateral filtering for now
                GPU_DEBUG_SCOPE(ReSTIRSSGIDenoise, "ReSTIRSSGIDenoise");

                CreateVS(vs, "ScreenPassVS", SHADER_SOURCE_PATH "screen_pass_v.glsl");
                CreatePS(ps, "ReSTIRSSGIDenoise", SHADER_SOURCE_PATH "ReSTIRSSGI_denoise_p.glsl");
                CreatePixelPipeline(pipeline, "ReSTIRSSGIDenoise", vs, ps);

                i32 src, dst;
                RenderTexture2D denoised[2] = { RenderTexture2D("ReSTIRSSGIResolvedIrradiance_0", render->indirectIrradiance()), RenderTexture2D("ReSTIRSSGIResolvedIrradiance_1", render->indirectIrradiance())};

                for (i32 pass = 0; pass < m_numDenoisingPass; ++pass)
                {
                    src = pass % 2;
                    dst = (pass + 1) % 2;

                    if (pass == 0)
                    {
                        renderer->blitTexture(denoised[src].getGfxTexture2D(), resolved.getGfxTexture2D());
                    }

                    renderer->drawFullscreenQuad(
                        getFramebufferSize(render->indirectIrradiance()),
                        [render, denoised, dst](RenderPass& pass) {
                            RenderTarget renderTarget(denoised[dst].getGfxTexture2D(), 0, glm::vec4(0.f, 0.f, 0.f, 1.f));
                            pass.setRenderTarget(renderTarget, 0);
                        },
                        pipeline,
                        [this, render, viewParameters, denoised, src](ProgramPipeline* p) {
                            viewParameters.setShaderParameters(p);
                            p->setTexture("sceneDepthBuffer", render->depth());
                            p->setTexture("sceneNormalBuffer", render->normal()); 
                            p->setTexture("indirectIrradiance", denoised[src].getGfxTexture2D());
                        }
                    );
                }

                renderer->blitTexture(render->indirectIrradiance(), denoised[dst].getGfxTexture2D());
            }
            else
            {
                renderer->blitTexture(render->indirectIrradiance(), resolved.getGfxTexture2D());
            }
        }
    }

#define IMGUI_CHECKBOX(name, var)           \
    ImGui::Text(name); ImGui::SameLine();   \
    ImGui::Checkbox("##" name, &var);       \

    void ReSTIRSSGIRenderer::renderUI()
    {
        if (ImGui::TreeNode("ReSTIRSSGI"))
        {
            ImGui::Text("Indirect Boost"); ImGui::SameLine();
            ImGui::SliderFloat("##Indirect Boost", &m_settings.indirectBoost, Settings::kMinIndirectBoost, Settings::kMaxIndirectBoost);

            IMGUI_CHECKBOX("Temporal Resampling", bTemporalResampling)

            IMGUI_CHECKBOX("Spatial Resampling", bSpatialResampling)
            ImGui::Text("Spatial Resampling Radius"); ImGui::SameLine();
            ImGui::SliderFloat("##Spatial Resampling Radius", &m_spatialReuseRadius, 0.f, 1.f);

            IMGUI_CHECKBOX("Denoising", bDenoisingPass)

            ImGui::TreePop();
        }
    }
}
