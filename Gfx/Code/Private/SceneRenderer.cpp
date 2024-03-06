#include "SceneRender.h"
#include "SceneRenderer.h"
#include "RenderPass.h"
#include "RenderTexture.h"
#include "Shader.h"
#include "Scene.h"
#include "GfxStaticMesh.h"
#include "GfxMaterial.h"
#include "RenderingUtils.h"
#include "ShadowMaps.h"

namespace Cyan
{
    SceneRenderer* SceneRenderer::s_renderer = nullptr;
    SceneRenderer::SceneRenderer()
    {
    }

    SceneRenderer* SceneRenderer::get()
    {
        static std::unique_ptr<SceneRenderer> s_instance(new SceneRenderer());
        if (s_renderer == nullptr)
        {
            s_renderer = s_instance.get();;
        }
        return s_renderer;
    }

    SceneRenderer::~SceneRenderer() { }

    void SceneRenderer::render(Scene* scene, SceneView& sceneView)
    {
        GHDepthTexture* sceneDepthTex = sceneView.m_render->depth();
        renderSceneDepth(sceneDepthTex, scene, sceneView.m_state);
        sceneView.flushTasks(SceneRenderingStage::kPostDepthPrepass);

        renderSceneGBuffer(scene, sceneView);
        renderSceneDirectLighting(scene, sceneView);

        if (scene->m_skybox != nullptr)
        {
            auto sceneDepthTex = sceneView.getRender()->depth();
            auto sceneDirectLighting = sceneView.getRender()->directLighting();
            const auto& viewState = sceneView.getState();
            RenderingUtils::renderSkybox(sceneDirectLighting, sceneDepthTex, scene->m_skybox, viewState.viewMatrix, viewState.projectionMatrix, 0);
        }

        renderSceneIndirectLighting(scene, sceneView);

        // combine direct and indirect lighting
        {
            GPU_DEBUG_SCOPE(marker, "Combine Lighting Pass")
            bool found = false;
            auto vs = ShaderManager::findShader<VertexShader>("ScreenPassVS");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "CombineLightingPS", ENGINE_SHADER_PATH "combine_lighting_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            auto directLighting = sceneView.getRender()->directLighting();
            auto indirectLighting = sceneView.getRender()->indirectLighting();

            auto outLighting = sceneView.getRender()->color();
            const auto desc = outLighting->getDesc();
            RenderingUtils::renderScreenPass(
                glm::uvec2(desc.width, desc.height),
                [outLighting](RenderPass& pass) {
                    RenderTarget rt(outLighting, 0);
                    pass.setRenderTarget(rt, 0);
                },
                gfxp.get(),
                [directLighting, indirectLighting](GfxPipeline* p) {
                    p->setTexture("u_directLighting", directLighting);
                    p->setTexture("u_indirectLighting", indirectLighting);
                }
            );
        }

        sceneView.flushTasks(SceneRenderingStage::kPostLighting);

        switch (m_settings.renderMode)
        {
        case RenderMode::kResolvedSceneColor:
            // postprocessing
            postprocessing(sceneView.m_render->resolvedColor(), sceneView.m_render->color());
            break;
        case RenderMode::kAlbedo:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->albedo(), 0);
            break;
        case RenderMode::kNormal:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->normal(), 0);
            break;
        case RenderMode::kDepth:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->depth(), 0);
            break;
        case RenderMode::kMetallicRoughness:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->metallicRoughness(), 0);
            break;
        case RenderMode::kSkyIrradiance:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->skyIrradiance(), 0);
            break;
        case RenderMode::kDirectLighting:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->directLighting(), 0);
            break;
        case RenderMode::kDirectDiffuseLighting:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->directDiffuseLighting(), 0);
            break;
        case RenderMode::kIndirectIrradiance:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->indirectIrradiance(), 0);
            break;
        case RenderMode::kIndirectLighting:
            RenderingUtils::copyTexture(sceneView.m_render->resolvedColor(), 0, sceneView.m_render->indirectLighting(), 0);
            break;
        default:
            assert(0);
            break;
        }

        RenderingUtils::copyDepthTexture(sceneView.m_render->prevFrameDepth(), sceneView.m_render->depth());
    }

    static void setShaderSceneViewInfo(GfxPipeline* gfxp, const SceneView::State& viewState)
    {
        gfxp->setUniform("viewParameters.renderResolution", viewState.resolution);
        gfxp->setUniform("viewParameters.aspectRatio", viewState.aspectRatio);
        gfxp->setUniform("viewParameters.viewMatrix", viewState.viewMatrix);
        gfxp->setUniform("viewParameters.prevFrameViewMatrix", viewState.prevFrameViewMatrix);
        gfxp->setUniform("viewParameters.projectionMatrix", viewState.projectionMatrix);
        gfxp->setUniform("viewParameters.prevFrameProjectionMatrix", viewState.prevFrameProjectionMatrix);
        gfxp->setUniform("viewParameters.cameraPosition", viewState.cameraPosition);
        gfxp->setUniform("viewParameters.prevFrameCameraPosition", viewState.prevFrameCameraPosition);
        gfxp->setUniform("viewParameters.cameraRight", viewState.cameraRight);
        gfxp->setUniform("viewParameters.cameraForward", viewState.cameraForward);
        gfxp->setUniform("viewParameters.cameraUp", viewState.cameraUp);
        gfxp->setUniform("viewParameters.frameCount", viewState.frameCount);
        gfxp->setUniform("viewParameters.elapsedTime", viewState.elapsedTime);
        gfxp->setUniform("viewParameters.deltaTime", viewState.deltaTime);
    }

    void SceneRenderer::renderSceneDepth(GHDepthTexture* outDepthTex, Scene* scene, const SceneView::State& viewState)
    {
        GPU_DEBUG_SCOPE(depthPass, "SceneDepthPass")

        if (outDepthTex != nullptr && scene != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "StaticMeshVS", ENGINE_SHADER_PATH "static_mesh_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "DepthPS", ENGINE_SHADER_PATH "depth_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            const auto desc = outDepthTex->getDesc();
            u32 width = desc.width, height = desc.height;
            RenderPass rp(width, height);
            rp.setDepthTarget(outDepthTex);
            rp.setRenderFunc([gfxp, scene, viewState](GfxContext* gfxc) {
                gfxp->bind();
                viewState.setShaderParameters(gfxp.get());
                for (auto instance : scene->m_staticSubMeshInstances)
                {
                    gfxp->setUniform("localToWorld", instance.localToWorldMatrix);
                    instance.subMesh->draw();
                }
                gfxp->unbind();
            });
            rp.enableDepthTest();
            rp.render(GfxContext::get());
        }
    }

    void SceneRenderer::renderSceneGBuffer(Scene* scene, SceneView& sceneView)
    {
        GPU_DEBUG_SCOPE(gbufferPass, "SceneGBufferPass")

        sceneView.flushTasks(SceneRenderingStage::kPreGBuffer);

        auto render = sceneView.m_render.get();
        /**
         * Assuming that there is always a depth prepass
         */
        auto inDepthTex = render->depth();
        auto outAlbedoTex = render->albedo();
        auto outNormalTex = render->normal();
        auto outMRTex = render->metallicRoughness();

        if (scene != nullptr && inDepthTex != nullptr && outAlbedoTex != nullptr && outNormalTex != nullptr && outMRTex != nullptr)
        {
            const auto desc = outAlbedoTex->getDesc();
            u32 width = desc.width, height = desc.height;
            RenderPass rp(width, height);

            DepthTarget depthTarget = { };
            depthTarget.depthTexture = inDepthTex;
            depthTarget.bNeedsClear = false;
            rp.setDepthTarget(depthTarget);
            rp.setRenderTarget(outAlbedoTex, 0);
            rp.setRenderTarget(outNormalTex, 1);
            rp.setRenderTarget(outMRTex, 2);

            const SceneView::State& viewState = sceneView.m_state;
            rp.setRenderFunc([scene, viewState](GfxContext* gfxc) {
                for (auto instance : scene->m_staticSubMeshInstances)
                {
                    auto gfxp = instance.material->bind();
                    viewState.setShaderParameters(gfxp);
                    gfxp->setUniform("localToWorld", instance.localToWorldMatrix);

                    instance.subMesh->draw();
                    instance.material->unbind();
                }
            });
            rp.enableDepthTest();
            rp.render(GfxContext::get());
        }

        sceneView.flushTasks(SceneRenderingStage::kPostGBuffer);
    }

    void SceneRenderer::renderSceneDirectLighting(Scene* scene, SceneView& sceneView)
    {
        GPU_DEBUG_SCOPE(gbufferPass, "SceneDirectLightingPass")

        // render directional shadow map
        sceneView.m_render->m_csm->render(scene, scene->m_directionalLight, sceneView.m_cameraInfo);

        auto outDirectLighting = sceneView.m_render->directLighting();
        auto outDirectDiffuseLighting = sceneView.m_render->directDiffuseLighting();
        auto depth = sceneView.m_render->depth();
        auto albedo = sceneView.m_render->albedo();
        auto normal = sceneView.m_render->normal();
        auto metallicRoughness = sceneView.m_render->metallicRoughness();
        auto csm = sceneView.m_render->m_csm.get();

        if (scene != nullptr && outDirectLighting != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "SceneDirectLightingPS", ENGINE_SHADER_PATH "scene_direct_lighting_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            const auto desc = outDirectLighting->getDesc();
            u32 width = desc.width, height = desc.height;
            const auto& viewState = sceneView.m_state;

            RenderingUtils::renderScreenPass(
                glm::uvec2(width, height),
                [outDirectLighting, outDirectDiffuseLighting](RenderPass& rp) {
                    rp.setRenderTarget(outDirectLighting, 0);
                    rp.setRenderTarget(outDirectDiffuseLighting, 1);
                },
                gfxp.get(),
                [scene, depth, normal, albedo, metallicRoughness, csm, viewState](GfxPipeline* p) {
                    viewState.setShaderParameters(p);

                    if (scene->m_directionalLight != nullptr)
                    {
                        p->setUniform("directionalLight.color", scene->m_directionalLight->m_color);
                        p->setUniform("directionalLight.intensity", scene->m_directionalLight->m_intensity);
                        p->setUniform("directionalLight.direction", scene->m_directionalLight->m_direction);
                        csm->setShaderParameters(p);
                    }

                    p->setTexture("sceneDepth", depth);
                    p->setTexture("sceneNormal", normal);
                    p->setTexture("sceneAlbedo", albedo);
                    p->setTexture("sceneMetallicRoughness", metallicRoughness);
                });
        }
    }

    static void renderSSAO(const SceneView::State& viewState, SceneRender* render)
    {
        {
            GPU_DEBUG_SCOPE(SSAOSampling, "SSAO Pass");

            const auto& aoDesc = render->ao()->getDesc();
            RenderTexture2D aoTemporalOutput("SSAOTemporalPassOutput", aoDesc);
            {
                GPU_DEBUG_SCOPE(SSAOSampling, "SSAO Temporal Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "GTAOPS", ENGINE_SHADER_PATH "gtao_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                RenderingUtils::renderScreenPass(
                    glm::uvec2(aoDesc.width, aoDesc.height),
                    [aoTemporalOutput](RenderPass& pass) {
                        RenderTarget rt(aoTemporalOutput.getGHTexture2D(), 0);
                        pass.setRenderTarget(rt, 0);
                    },
                    gfxp.get(),
                    [viewState, render](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_prevFrameSceneDepthTex", render->prevFrameDepth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_AOHistoryTex", render->aoHistory());

                        auto& blueNoiseTextures = RenderingUtils::get()->getNoiseTextures();
                        p->setTexture("u_blueNoiseTex_16x16_R[0]", blueNoiseTextures.blueNoise16x16_R_0.get());
                        p->setTexture("u_blueNoiseTex_16x16_R[1]", blueNoiseTextures.blueNoise16x16_R_1.get());
                        p->setTexture("u_blueNoiseTex_16x16_R[2]", blueNoiseTextures.blueNoise16x16_R_2.get());
                        p->setTexture("u_blueNoiseTex_16x16_R[3]", blueNoiseTextures.blueNoise16x16_R_3.get());
                        p->setTexture("u_blueNoiseTex_16x16_R[4]", blueNoiseTextures.blueNoise16x16_R_4.get());
                        p->setTexture("u_blueNoiseTex_16x16_R[5]", blueNoiseTextures.blueNoise16x16_R_5.get());
                        p->setTexture("u_blueNoiseTex_16x16_R[6]", blueNoiseTextures.blueNoise16x16_R_6.get());
                        p->setTexture("u_blueNoiseTex_16x16_R[7]", blueNoiseTextures.blueNoise16x16_R_7.get());
                        p->setTexture("u_blueNoiseTex_1024x1024_RGBA", blueNoiseTextures.blueNoise1024x1024_RGBA.get());
                    }
                );

                RenderingUtils::copyTexture(render->aoHistory(), 0, aoTemporalOutput.getGHTexture2D(), 0);
            }

            {
                GPU_DEBUG_SCOPE(SSAOSampling, "SSAO Spatial Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "SSAOFilteringPS", ENGINE_SHADER_PATH "ssao_filtering_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                RenderingUtils::renderScreenPass(
                    glm::uvec2(aoDesc.width, aoDesc.height),
                    [render](RenderPass& pass) {
                        RenderTarget rt(render->ao(), 0);
                        pass.setRenderTarget(rt, 0);
                    },
                    gfxp.get(),
                    [viewState, render, aoTemporalOutput](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_aoTemporalOutput", aoTemporalOutput.getGHTexture2D());
                    }
                );
            }
        }
    }

    // This pass require ao to be rendered prior.
    static void renderBasicImageBasedLighting(const SceneView::State& viewState, SceneRender* render, SkyLight* skyLight)
    {
        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "SceneDirectSkyLightPS", ENGINE_SHADER_PATH "scene_direct_skylight_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        const auto desc = render->skyIrradiance()->getDesc();
        u32 width = desc.width, height = desc.height;

        RenderingUtils::renderScreenPass(
            glm::uvec2(width, height),
            [render](RenderPass& rp) {
                rp.setRenderTarget(render->skyIrradiance(), 0);
            },
            gfxp.get(),
            [render, viewState, skyLight](GfxPipeline* p) {
                viewState.setShaderParameters(p);

                p->setTexture("u_sceneDepth", render->depth());
                p->setTexture("u_sceneNormal", render->normal());
                p->setTexture("u_sceneAlbedo", render->albedo());
                p->setTexture("u_sceneMetallicRoughness", render->metallicRoughness());
                auto irradianceCubemap = skyLight->getIrradianceCubemap();
                p->setTexture("u_irradianceCubemap", irradianceCubemap->cubemap.get());
                auto reflectionCubemap = skyLight->getReflectionCubemap();
                p->setTexture("u_reflectionCubemap", reflectionCubemap->cubemap.get());
                p->setTexture("u_BRDFLUT", ReflectionCubemap::BRDFLUT.get());
                p->setTexture("u_SSAOTex", render->ao());
            }
        );

        RenderingUtils::copyTexture(render->prevFrameSkyIrradiance(), 0, render->skyIrradiance(), 0);
    }

    // This pass require hiz to be built prior.
    static void renderNaiveSSDO(const SceneView::State& viewState, SceneRender* render, SkyLight* skyLight)
    {
        GPU_DEBUG_SCOPE(SSDOPass, "Naive SSDO");

        bool found = false;
        auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
        auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "SceneDirectSkyLightPS", ENGINE_SHADER_PATH "naive_ssdo_p.glsl");
        auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

        GfxContext* ctx = GfxContext::get();
        ctx->enableBlending();
        ctx->setBlendingMode(BlendingMode::kAdditive);

        const auto desc = render->skyIrradiance()->getDesc();
        glm::uvec2 renderResolution(desc.width, desc.height);
        RenderingUtils::renderScreenPass(
            renderResolution,
            [render](RenderPass& rp) {
                rp.setRenderTarget(render->skyIrradiance(), 0);
                rp.setRenderTarget(render->skyReflection(), 1);

                RenderTarget directLightingRT(render->directLighting(), 0, false);
                rp.setRenderTarget(directLightingRT, 2);

                RenderTarget directDiffuseLightingRT(render->directDiffuseLighting(), 0, false);
                rp.setRenderTarget(directDiffuseLightingRT, 3);
            },
            gfxp.get(),
            [skyLight, render, viewState](GfxPipeline* p) {
                viewState.setShaderParameters(p);

                p->setTexture("u_sceneDepthTex", render->depth());
                p->setTexture("u_prevFrameSceneDepthTex", render->prevFrameDepth());
                p->setTexture("u_sceneNormalTex", render->normal());
                p->setTexture("u_sceneAlbedoTex", render->albedo());
                p->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());
                p->setTexture("u_skyboxCubemap", skyLight->getReflectionCubemap()->cubemap.get());
                p->setTexture("u_BRDFLUT", ReflectionCubemap::BRDFLUT.get());
                p->setTexture("u_prevFrameSkyIrradianceTex", render->prevFrameSkyIrradiance());

                auto hiz = render->hiz();
                p->setTexture("hiz.depthQuadtree", hiz->m_depthBuffer.get());
                p->setUniform("hiz.numMipLevels", (i32)(hiz->m_depthBuffer->getDesc().numMips));
                p->setUniform("settings.kMaxNumIterationsPerRay", 64);
            }
        );

        ctx->disableBlending();

        RenderingUtils::copyTexture(render->prevFrameSkyIrradiance(), 0, render->skyIrradiance(), 0);
    }

    static void renderNaiveSSGI(const SceneView::State& viewState, Scene* scene, SceneRender* render)
    {
        render->hiz()->build(render->depth());
        renderNaiveSSDO(viewState, render, scene->m_skyLight);

        {
            GPU_DEBUG_SCOPE(SSGIDiffuse, "SSGI");

            {
                GPU_DEBUG_SCOPE(SSGINaiveDiffuse, "Naive Irradiance Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "SSGIDiffuseNaive", ENGINE_SHADER_PATH "ssgi_diffuse_naive_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                auto outIndirectIrradiance = render->indirectIrradiance();
                auto prevFrameIndirectIrradiance = render->prevFrameIndirectIrradiance();

                const auto& desc = outIndirectIrradiance->getDesc();
                GfxContext* ctx = GfxContext::get();
                ctx->enableBlending();
                ctx->setBlendingMode(BlendingMode::kAdditive);

                RenderingUtils::renderScreenPass(
                    glm::uvec2(desc.width, desc.height),
                    [outIndirectIrradiance, render](RenderPass& pass) {
                        RenderTarget indirectIrradianceRT(outIndirectIrradiance, 0);
                        pass.setRenderTarget(indirectIrradianceRT, 0);

                        RenderTarget indirectLightingRT(render->indirectLighting(), 0);
                        pass.setRenderTarget(indirectLightingRT, 1);
                    },
                    gfxp.get(),
                    [prevFrameIndirectIrradiance, render, viewState](GfxPipeline* p) {
                        viewState.setShaderParameters(p);
                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_prevFrameSceneDepthTex", render->prevFrameDepth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_sceneAlbedoTex", render->albedo());
                        p->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());
                        p->setTexture("u_prevFrameIndirectIrradianceTex", render->prevFrameIndirectIrradiance());
                        p->setTexture("u_diffuseRadianceTex", render->directDiffuseLighting());

                        auto hiz = render->hiz();
                        p->setTexture("hiz.depthQuadtree", hiz->m_depthBuffer.get());
                        p->setUniform("hiz.numMipLevels", (i32)(hiz->m_depthBuffer->getDesc().numMips));
                        p->setUniform("settings.kMaxNumIterationsPerRay", 64);
                    }
                );

                ctx->disableBlending();

                RenderingUtils::copyTexture(render->prevFrameIndirectIrradiance(), 0, render->indirectIrradiance(), 0);
            }
        }
    }

    // reference iq's restir ao implementation here https://www.shadertoy.com/view/stVfWc
    static void renderReSTIRSSGI(SceneRenderer* renderer, const SceneView::State& viewState, Scene* scene, SceneRender* render)
    {
        renderSSAO(viewState, render);

        GPU_DEBUG_SCOPE(ReSTIRSSGI, "ReSTIR SSGI");

        render->hiz()->build(render->depth());

        const auto& reservoirPositionDesc = render->reservoirPosition()->getDesc();
        RenderTexture2D screenSpaceHitTexture("ReSTIRScreenSpaceHitPosition", reservoirPositionDesc);

        if (scene->m_skyLight != nullptr)
        {
            const auto desc = render->reservoirSkyRadiance()->getDesc();
            auto skyLight = scene->m_skyLight;
            {
                GPU_DEBUG_SCOPE(ReSTIRSSDOTemporal, "ReSTIR SSDO Temporal Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSTIRSSDOTemporalPS", ENGINE_SHADER_PATH "restir_ssdo_temporal_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                glm::uvec2 renderResolution(desc.width, desc.height);
                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render, screenSpaceHitTexture](RenderPass& rp) {
                        RenderTarget reservoirRadianceRT(render->reservoirSkyRadiance(), 0, false);
                        RenderTarget reservoirDirectionRT(render->reservoirSkyDirection(), 0, false);
                        RenderTarget reservoirWSumMWRT(render->reservoirSkyWSumMW(), 0, false);
                        rp.setRenderTarget(reservoirRadianceRT, 0);
                        rp.setRenderTarget(reservoirDirectionRT, 1);
                        rp.setRenderTarget(reservoirWSumMWRT, 2);
                        rp.setRenderTarget(screenSpaceHitTexture.getGHTexture2D(), 3);
                    },
                    gfxp.get(),
                    [skyLight, render, viewState](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_prevFrameSceneDepthTex", render->prevFrameDepth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_sceneAlbedoTex", render->albedo());
                        p->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());
                        p->setTexture("u_skyboxCubemap", skyLight->getReflectionCubemap()->cubemap.get());
                        p->setTexture("u_BRDFLUT", ReflectionCubemap::BRDFLUT.get());
                        p->setTexture("u_prevFrameSkyIrradianceTex", render->prevFrameSkyIrradiance());

                        p->setTexture("u_prevFrameReservoirSkyRadiance", render->prevFrameReservoirSkyRadiance());
                        p->setTexture("u_prevFrameReservoirSkyDirection", render->prevFrameReservoirSkyDirection());
                        p->setTexture("u_prevFrameReservoirSkyWSumMW", render->prevFrameReservoirSkyWSumMW());

                        auto hiz = render->hiz();
                        p->setTexture("hiz.depthQuadtree", hiz->m_depthBuffer.get());
                        p->setUniform("hiz.numMipLevels", (i32)(hiz->m_depthBuffer->getDesc().numMips));
                        p->setUniform("settings.kMaxNumIterationsPerRay", 64);
                    }
                );

                RenderingUtils::copyTexture(render->prevFrameReservoirSkyRadiance(), 0, render->reservoirSkyRadiance(), 0);
                RenderingUtils::copyTexture(render->prevFrameReservoirSkyDirection(), 0, render->reservoirSkyDirection(), 0);
                RenderingUtils::copyTexture(render->prevFrameReservoirSkyWSumMW(), 0, render->reservoirSkyWSumMW(), 0);
            }

            RenderTexture2D spatialReservoirSkyRadiance_0("SpatialReservoirSkyRadiance_0", desc);
            RenderTexture2D spatialReservoirSkyDirection_0("SpatialReservoirSkyDirection_0", desc);
            RenderTexture2D spatialReservoirSkyWSumMW_0("SpatialReservoirSkyWSumMW_0", desc);

            RenderTexture2D spatialReservoirSkyRadiance_1("SpatialReservoirSkyRadiance_1", desc);
            RenderTexture2D spatialReservoirSkyDirection_1("SpatialReservoirSkyDirection_1", desc);
            RenderTexture2D spatialReservoirSkyWSumMW_1("SpatialReservoirSkyWSumMW_1", desc);

            if (renderer->m_settings.ReSTIRSSDOSettings.bEnableSpatialPass)
            {
                GPU_DEBUG_SCOPE(ReSTIRSSDOSpatial, "ReSTIR SSDO Spatial Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSTIRSSDOSpatialPS", ENGINE_SHADER_PATH "restir_ssdo_spatial_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                glm::uvec2 renderResolution(desc.width, desc.height);

                // first pass
                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render, spatialReservoirSkyRadiance_0, spatialReservoirSkyDirection_0, spatialReservoirSkyWSumMW_0](RenderPass& rp) {
                        RenderTarget reservoirRadianceRT(spatialReservoirSkyRadiance_0.getGHTexture2D(), 0);
                        RenderTarget reservoirDirectionRT(spatialReservoirSkyDirection_0.getGHTexture2D(), 0);
                        RenderTarget reservoirWSumMWRT(spatialReservoirSkyWSumMW_0.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirRadianceRT, 0);
                        rp.setRenderTarget(reservoirDirectionRT, 1);
                        rp.setRenderTarget(reservoirWSumMWRT, 2);
                    },
                    gfxp.get(),
                    [skyLight, render, viewState, renderer](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_reservoirSkyRadiance", render->reservoirSkyRadiance());
                        p->setTexture("u_reservoirSkyDirection", render->reservoirSkyDirection());
                        p->setTexture("u_reservoirSkyWSumMW", render->prevFrameReservoirSkyWSumMW());

                        p->setUniform("u_passIndex", 0);
                        p->setUniform("u_sampleCount", renderer->m_settings.ReSTIRSSDOSettings.spatialPassSampleCount);
                        p->setUniform("u_kernelRadius", renderer->m_settings.ReSTIRSSDOSettings.spatialPassKernelRadius);
                    }
                );

                // todo: this second pass is leading to some fireflies
                // second pass
                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render, spatialReservoirSkyRadiance_1, spatialReservoirSkyDirection_1, spatialReservoirSkyWSumMW_1](RenderPass& rp) {
                        RenderTarget reservoirRadianceRT(spatialReservoirSkyRadiance_1.getGHTexture2D(), 0);
                        RenderTarget reservoirDirectionRT(spatialReservoirSkyDirection_1.getGHTexture2D(), 0);
                        RenderTarget reservoirWSumMWRT(spatialReservoirSkyWSumMW_1.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirRadianceRT, 0);
                        rp.setRenderTarget(reservoirDirectionRT, 1);
                        rp.setRenderTarget(reservoirWSumMWRT, 2);
                    },
                    gfxp.get(),
                    [skyLight, render, viewState, renderer, spatialReservoirSkyRadiance_0, spatialReservoirSkyDirection_0, spatialReservoirSkyWSumMW_0](GfxPipeline* p) {
                        viewState.setShaderParameters(p);
                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_reservoirSkyRadiance", spatialReservoirSkyRadiance_0.getGHTexture2D());
                        p->setTexture("u_reservoirSkyDirection", spatialReservoirSkyDirection_0.getGHTexture2D());
                        p->setTexture("u_reservoirSkyWSumMW", spatialReservoirSkyWSumMW_0.getGHTexture2D());

                        p->setUniform("u_passIndex", 1);
                        p->setUniform("u_sampleCount", renderer->m_settings.ReSTIRSSDOSettings.spatialPassSampleCount);
                        p->setUniform("u_kernelRadius", renderer->m_settings.ReSTIRSSDOSettings.spatialPassKernelRadius * 2.f);
                    }
                );
            }

            {
                GPU_DEBUG_SCOPE(ReSTIRSSDOTemporal, "ReSTIR SSDO Estimate Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSTIRSSDOEstimatePS", ENGINE_SHADER_PATH "restir_ssdo_estimate_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                const auto desc = render->skyIrradiance()->getDesc();
                glm::uvec2 renderResolution(desc.width, desc.height);

                GfxContext* ctx = GfxContext::get();
                ctx->enableBlending();
                ctx->setBlendingMode(BlendingMode::kAdditive);

                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render](RenderPass& rp) {
                        RenderTarget skyIrradianceRT(render->skyIrradiance(), 0);
                        rp.setRenderTarget(skyIrradianceRT, 0);

                        RenderTarget directLightingRT(render->directLighting(), 0, false);
                        rp.setRenderTarget(directLightingRT, 1);

                        RenderTarget directDiffuseLightingRT(render->directDiffuseLighting(), 0, false);
                        rp.setRenderTarget(directDiffuseLightingRT, 2);
                    },
                    gfxp.get(),
                    [skyLight, renderer, render, viewState, spatialReservoirSkyDirection_1, spatialReservoirSkyRadiance_1, spatialReservoirSkyWSumMW_1](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_sceneAlbedoTex", render->albedo());
                        p->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());
                        p->setUniform("u_applyAO", renderer->m_settings.ReSTIRSSDOSettings.bApplySSAO ? 1.f : 0.f);
                        p->setTexture("u_SSAOTex", render->ao());

                        if (renderer->m_settings.ReSTIRSSDOSettings.bEnableSpatialPass)
                        {
                            p->setTexture("u_reservoirSkyRadiance", spatialReservoirSkyRadiance_1.getGHTexture2D());
                            p->setTexture("u_reservoirSkyDirection", spatialReservoirSkyDirection_1.getGHTexture2D());
                            p->setTexture("u_reservoirSkyWSumMW", spatialReservoirSkyWSumMW_1.getGHTexture2D());
                        }
                        else
                        {
                            p->setTexture("u_reservoirSkyRadiance", render->reservoirSkyRadiance());
                            p->setTexture("u_reservoirSkyDirection", render->reservoirSkyDirection());
                            p->setTexture("u_reservoirSkyWSumMW", render->reservoirSkyWSumMW());
                        }
                    }
                );

                ctx->disableBlending();

                RenderingUtils::copyTexture(render->prevFrameSkyIrradiance(), 0, render->skyIrradiance(), 0);
            }
        }

        if (renderer->m_settings.ReSTIRSSGISettings.bEnable)
        {
            // temporal pass
            {
                GPU_DEBUG_SCOPE(ReSTIRSSGI, "ReSTIR Indirect Diffuse Temporal Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSITRIndirectDiffuseTemporalPS", ENGINE_SHADER_PATH "restir_indirect_diffuse_temporal_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                const auto& desc = render->reservoirRadiance()->getDesc();
                glm::uvec2 renderResolution(desc.width, desc.height);
                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render](RenderPass& rp) {
                        RenderTarget reservoirRadianceRT(render->reservoirRadiance(), 0);
                        rp.setRenderTarget(reservoirRadianceRT, 0);

                        RenderTarget reservoirPositionRT(render->reservoirPosition(), 0);
                        rp.setRenderTarget(reservoirPositionRT, 1);

                        RenderTarget reservoirNormalRT(render->reservoirNormal(), 0);
                        rp.setRenderTarget(reservoirNormalRT, 2);

                        RenderTarget reservoirWSumMWRT(render->reservoirWSumMW(), 0);
                        rp.setRenderTarget(reservoirWSumMWRT, 3);
                    },
                    gfxp.get(),
                    [renderer, render, viewState, screenSpaceHitTexture](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_diffuseRadianceTex", render->directDiffuseLighting());
                        p->setTexture("u_screenSpaceHitPositionTex", screenSpaceHitTexture.getGHTexture2D());
                        
                        if (viewState.frameCount > 0)
                        {
                            p->setTexture("u_prevFrameReservoirRadiance", render->prevFrameReservoirRadiance());
                            p->setTexture("u_prevFrameReservoirPosition", render->prevFrameReservoirPosition());
                            p->setTexture("u_prevFrameReservoirNormal", render->prevFrameReservoirNormal());
                            p->setTexture("u_prevFrameReservoirWSumMW", render->prevFrameReservoirWSumMW());
                            p->setTexture("u_prevFrameSceneDepthTex", render->prevFrameDepth());
                        }
                    }
                );

                RenderingUtils::copyTexture(render->prevFrameReservoirRadiance(), 0, render->reservoirRadiance(), 0);
                RenderingUtils::copyTexture(render->prevFrameReservoirPosition(), 0, render->reservoirPosition(), 0);
                RenderingUtils::copyTexture(render->prevFrameReservoirNormal(), 0, render->reservoirNormal(), 0);
                RenderingUtils::copyTexture(render->prevFrameReservoirWSumMW(), 0, render->reservoirWSumMW(), 0);
            }

            RenderTexture2D spatialReservoirRadiance("SpatialReservoirRadiance", render->reservoirRadiance()->getDesc());
            RenderTexture2D spatialReservoirPosition("SpatialReservoirPosition", render->reservoirPosition()->getDesc());
            RenderTexture2D spatialReservoirNormal("SpatialReservoirNormal", render->reservoirNormal()->getDesc());
            RenderTexture2D spatialReservoirWSumMW("SpatialReservoirWSumMW", render->reservoirWSumMW()->getDesc());

            RenderTexture2D spatialReservoirRadiance_1("SpatialReservoirRadiance_1", render->reservoirRadiance()->getDesc());
            RenderTexture2D spatialReservoirPosition_1("SpatialReservoirPosition_1", render->reservoirPosition()->getDesc());
            RenderTexture2D spatialReservoirNormal_1("SpatialReservoirNormal_1", render->reservoirNormal()->getDesc());
            RenderTexture2D spatialReservoirWSumMW_1("SpatialReservoirWSumMW_1", render->reservoirWSumMW()->getDesc());

            if (renderer->m_settings.ReSTIRSSGISettings.bEnableSpatialPass)
            {
                GPU_DEBUG_SCOPE(ReSTIRSSGI, "ReSTIR Indirect Diffuse Spatial Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSITRIndirectDiffuseSpatialPS", ENGINE_SHADER_PATH "restir_indirect_diffuse_spatial_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                const auto& desc = render->reservoirRadiance()->getDesc();
                glm::uvec2 renderResolution(desc.width, desc.height);
                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render, spatialReservoirRadiance, spatialReservoirPosition, spatialReservoirNormal, spatialReservoirWSumMW](RenderPass& rp) {
                        RenderTarget reservoirRadianceRT(spatialReservoirRadiance.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirRadianceRT, 0);

                        RenderTarget reservoirPositionRT(spatialReservoirPosition.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirPositionRT, 1);

                        RenderTarget reservoirNormalRT(spatialReservoirNormal.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirNormalRT, 2);

                        RenderTarget reservoirWSumMWRT(spatialReservoirWSumMW.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirWSumMWRT, 3);
                    },
                    gfxp.get(),
                    [renderer, render, viewState, screenSpaceHitTexture](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_reservoirRadiance", render->reservoirRadiance());
                        p->setTexture("u_reservoirPosition", render->reservoirPosition());
                        p->setTexture("u_reservoirNormal", render->reservoirNormal());
                        p->setTexture("u_reservoirWSumMW", render->reservoirWSumMW());
                        p->setUniform("u_sampleCount", renderer->m_settings.ReSTIRSSGISettings.spatialPassSampleCount);
                        p->setUniform("u_kernelRadius", renderer->m_settings.ReSTIRSSGISettings.spatialPassKernelRadius);
                    }
                );

                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render, spatialReservoirRadiance_1, spatialReservoirPosition_1, spatialReservoirNormal_1, spatialReservoirWSumMW_1](RenderPass& rp) {
                        RenderTarget reservoirRadianceRT(spatialReservoirRadiance_1.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirRadianceRT, 0);

                        RenderTarget reservoirPositionRT(spatialReservoirPosition_1.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirPositionRT, 1);

                        RenderTarget reservoirNormalRT(spatialReservoirNormal_1.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirNormalRT, 2);

                        RenderTarget reservoirWSumMWRT(spatialReservoirWSumMW_1.getGHTexture2D(), 0);
                        rp.setRenderTarget(reservoirWSumMWRT, 3);
                    },
                    gfxp.get(),
                    [renderer, render, viewState, spatialReservoirRadiance, spatialReservoirPosition, spatialReservoirNormal, spatialReservoirWSumMW](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_reservoirRadiance", spatialReservoirRadiance.getGHTexture2D());
                        p->setTexture("u_reservoirPosition", spatialReservoirPosition.getGHTexture2D());
                        p->setTexture("u_reservoirNormal", spatialReservoirNormal.getGHTexture2D());
                        p->setTexture("u_reservoirWSumMW", spatialReservoirWSumMW.getGHTexture2D());
                        p->setUniform("u_sampleCount", renderer->m_settings.ReSTIRSSGISettings.spatialPassSampleCount);
                        p->setUniform("u_kernelRadius", renderer->m_settings.ReSTIRSSGISettings.spatialPassKernelRadius * 2.f);
                    }
                );
            }

            RenderTexture2D unfilteredIndirectIrradiance("UnfilteredIndirectIrradiance", render->indirectIrradiance()->getDesc());

            // resolve pass
            {
                GPU_DEBUG_SCOPE(ReSTIRSSGI, "ReSTIR Indirect Diffuse Estimate Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSITRIndirectDiffuseEstimatePS", ENGINE_SHADER_PATH "restir_indirect_diffuse_estimate_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                const auto& desc = unfilteredIndirectIrradiance.getGHTexture2D()->getDesc();
                glm::uvec2 renderResolution(desc.width, desc.height);
                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render, unfilteredIndirectIrradiance](RenderPass& rp) {
                        RenderTarget indirectIrradianceRT(unfilteredIndirectIrradiance.getGHTexture2D(), 0);
                        rp.setRenderTarget(indirectIrradianceRT, 0);
                    },
                    gfxp.get(),
                    [renderer, render, viewState, spatialReservoirRadiance_1, spatialReservoirPosition_1, spatialReservoirNormal_1, spatialReservoirWSumMW_1](GfxPipeline* p) {
                        viewState.setShaderParameters(p);

                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneNormalTex", render->normal());
                        p->setTexture("u_sceneAlbedoTex", render->albedo());
                        p->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());

                        if (renderer->m_settings.ReSTIRSSGISettings.bEnableSpatialPass)
                        {
                            p->setTexture("u_reservoirRadiance", spatialReservoirRadiance_1.getGHTexture2D());
                            p->setTexture("u_reservoirPosition", spatialReservoirPosition_1.getGHTexture2D());
                            p->setTexture("u_reservoirNormal", spatialReservoirNormal_1.getGHTexture2D());
                            p->setTexture("u_reservoirWSumMW", spatialReservoirWSumMW_1.getGHTexture2D());
                        }
                        else
                        {
                            p->setTexture("u_reservoirRadiance", render->reservoirRadiance());
                            p->setTexture("u_reservoirPosition", render->reservoirPosition());
                            p->setTexture("u_reservoirNormal", render->reservoirNormal());
                            p->setTexture("u_reservoirWSumMW", render->reservoirWSumMW());
                        }
                    }
                );
            }

            RenderTexture2D denoisedIrradiance[2] = {
                 RenderTexture2D("DenoisedIrradiance_0", render->indirectIrradiance()->getDesc()),
                 RenderTexture2D("DenoisedIrradiance_1", render->indirectIrradiance()->getDesc())
            };
            i32 src = 0, dst = 0;

            // filtering pass using A-Trous edge avoiding filter
            // todo: edge stopping functions: normal, and indirect irradiance
            {
                GPU_DEBUG_SCOPE(ReSTIRSSGI, "ReSTIR Indirect Diffuse Filtering Pass");

                struct ATrousFilter
                {
                    ATrousFilter()
                    {
                        // calculate 5x5 kernel weights
                        f32 wSum = 0.f;
                        for (i32 y = 0; y < 5; ++y)
                        {
                            for (i32 x = 0; x < 5; ++x)
                            {
                                f32 w = h[x] * h[y];
                                kernel[y * 5 + x] = w;
                                wSum += w;
                            }
                        }
                        for (i32 i = 0; i < 25; ++i)
                        {
                            kernel[i] /= wSum;
                        }
                        for (i32 y = 0; y < 5; ++y)
                        {
                            for (i32 x = 0; x < 5; ++x)
                            {
                                printf("%.4f, ", kernel[y * 5 + x]);
                            }
                            printf("\n");
                        }
                    }

                    const f32 h[5] = { 1.f / 16.f, 1.f / 4.f, 3.f / 8.f, 1.f / 4.f, 1.f / 16.f };
                    f32 kernel[25];
                    glm::vec2 offsets[25] = {
                          glm::vec2(-2.f, 2.f), glm::vec2(-1.f, 2.f), glm::vec2(0.f, 2.f), glm::vec2(1.f, 2.f), glm::vec2(2.f, 2.f),
                          glm::vec2(-2.f, 1.f), glm::vec2(-1.f, 1.f), glm::vec2(0.f, 1.f), glm::vec2(1.f, 1.f), glm::vec2(2.f, 1.f),
                          glm::vec2(-2.f, 0.f), glm::vec2(-1.f, 0.f), glm::vec2(0.f, 0.f), glm::vec2(1.f, 0.f), glm::vec2(2.f, 0.f),
                          glm::vec2(-2.f,-1.f), glm::vec2(-1.f,-1.f), glm::vec2(0.f,-1.f), glm::vec2(1.f,-1.f), glm::vec2(2.f,-1.f),
                          glm::vec2(-2.f,-2.f), glm::vec2(-1.f,-2.f), glm::vec2(0.f,-2.f), glm::vec2(1.f,-2.f), glm::vec2(2.f,-2.f)
                    };
                };

                static ATrousFilter filter;

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSITRIndirectDiffuseDenoisePS", ENGINE_SHADER_PATH "restir_indirect_diffuse_denoise_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                RenderingUtils::copyTexture(denoisedIrradiance[0].getGHTexture2D(), 0, unfilteredIndirectIrradiance.getGHTexture2D(), 0);

                for (i32 pass = 0; pass < renderer->m_settings.ReSTIRSSGISettings.numDenoisingPasses; ++pass)
                {
                    src = pass % 2;
                    dst = (src + 1) % 2;

                    const auto& desc = render->indirectIrradiance()->getDesc();
                    glm::uvec2 renderResolution(desc.width, desc.height);
                    RenderingUtils::renderScreenPass(
                        renderResolution,
                        [render, dst, denoisedIrradiance](RenderPass& rp) {
                            RenderTarget rt(denoisedIrradiance[dst].getGHTexture2D(), 0);
                            rp.setRenderTarget(rt, 0);
                        },
                        gfxp.get(),
                        [renderer, render, viewState, denoisedIrradiance, src, pass, renderResolution](GfxPipeline* p) {
                            viewState.setShaderParameters(p);

                            p->setTexture("u_indirectIrradianceTex", denoisedIrradiance[src].getGHTexture2D());
                            p->setTexture("u_sceneDepthTex", render->depth());
                            p->setTexture("u_sceneNormalTex", render->normal());
                            for (i32 i = 0; i < 25; ++i)
                            {
                                std::string kernelName = "u_kernel[" + std::to_string(i) + "]";
                                p->setUniform(kernelName.c_str(), filter.kernel[i]);

                                std::string offsetName = "u_offsets[" + std::to_string(i) + "]";
                                p->setUniform(offsetName.c_str(), filter.offsets[i]);
                            }
                            p->setUniform("u_passIndex", pass);
                            p->setUniform("u_renderResolution", renderResolution);

                            p->setUniform("u_normalEdgeStopping", renderer->m_settings.ReSTIRSSGISettings.bNormalEdgeStopping ? 1.f : 0.f);
                            p->setUniform("u_sigmaN", renderer->m_settings.ReSTIRSSGISettings.sigmaN);

                            p->setUniform("u_positionEdgeStopping", renderer->m_settings.ReSTIRSSGISettings.bPositionEdgeStopping ? 1.f : 0.f);
                            p->setUniform("u_sigmaP", renderer->m_settings.ReSTIRSSGISettings.sigmaP);
                        }
                    );
                }
            }

            // final blending pass
            {
                GPU_DEBUG_SCOPE(ReSTIRSSGI, "ReSTIR Indirect Diffuse Compositing Pass");

                bool found = false;
                auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
                auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "ReSITRIndirectDiffuseCompositePS", ENGINE_SHADER_PATH "restir_indirect_diffuse_composite_p.glsl");
                auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

                GfxContext* ctx = GfxContext::get();
                ctx->enableBlending();
                ctx->setBlendingMode(BlendingMode::kAdditive);

                const auto& desc = render->indirectLighting()->getDesc();
                glm::uvec2 renderResolution(desc.width, desc.height);
                RenderingUtils::renderScreenPass(
                    renderResolution,
                    [render](RenderPass& rp) {
                        RenderTarget indirectIrradianceRT(render->indirectIrradiance(), 0);
                        rp.setRenderTarget(indirectIrradianceRT, 0);

                        RenderTarget indirectLightingRT(render->indirectLighting(), 0);
                        rp.setRenderTarget(indirectLightingRT, 1);
                    },
                    gfxp.get(),
                    [renderer, render, viewState, denoisedIrradiance, dst](GfxPipeline* p) {
                        p->setTexture("u_indirectIrradianceTex", denoisedIrradiance[dst].getGHTexture2D());
                        p->setUniform("u_indirectIrradianceBoost", renderer->m_settings.ReSTIRSSGISettings.indirectIrradianceBoost);
                        p->setTexture("u_sceneAlbedoTex", render->albedo());
                        p->setTexture("u_sceneDepthTex", render->depth());
                        p->setTexture("u_sceneMetallicRoughnessTex", render->metallicRoughness());
                    }
                );

                ctx->disableBlending();
            }
        }
    }

    void SceneRenderer::renderSceneIndirectLighting(Scene* scene, SceneView& sceneView)
    {
        GPU_DEBUG_SCOPE(IndirectLightingPass, "SceneIndirectLightingPass");

        auto render = sceneView.getRender();
        const auto& viewState = sceneView.getState();

        RenderingUtils::clearTexture2D(render->indirectLighting(), glm::vec4(0.f, 0.f, 0.f, 1.f));

        if (scene->m_skyLight != nullptr)
        {
            // renderNaiveSSGI(viewState, scene, render);
            // todo: restir version
            // renderReSTIRSSGI(this, viewState, scene, render);
            renderBasicImageBasedLighting(viewState, render, scene->m_skyLight);
        }
    }

    void SceneRenderer::postprocessing(GHTexture2D* dstTexture, GHTexture2D* srcTexture)
    {
        RenderingUtils::tonemapping(dstTexture, srcTexture);
#if 0
        if (dstTexture != nullptr && srcTexture != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", ENGINE_SHADER_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "TonemappingPS", ENGINE_SHADER_PATH "tonemapping_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            const auto desc = dstTexture->getDesc();
            u32 width = desc.width, height = desc.height;
            RenderingUtils::renderScreenPass(
                glm::uvec2(width, height),
                [srcTexture, dstTexture](RenderPass& rp) {
                    rp.setRenderTarget(dstTexture, 0);
                },
                gfxp.get(),
                [srcTexture, dstTexture](GfxPipeline* p) {
                    p->setTexture("srcTexture", srcTexture);
                });
        }
#endif
    }
}