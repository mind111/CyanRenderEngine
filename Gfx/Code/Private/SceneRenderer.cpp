#include "SceneRender.h"
#include "SceneRenderer.h"
#include "RenderPass.h"
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
        renderSceneGBuffer(scene, sceneView);
        renderSceneDirectLighting(scene, sceneView);

        // postprocessing
        tonemapping(sceneView.m_render->resolvedColor(), sceneView.m_render->directLighting());
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
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "StaticMeshVS", SHADER_TEXT_PATH "static_mesh_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "DepthPS", SHADER_TEXT_PATH "depth_p.glsl");
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
    }

    void SceneRenderer::renderSceneDirectLighting(Scene* scene, SceneView& sceneView)
    {
        GPU_DEBUG_SCOPE(gbufferPass, "SceneDirectLightingPass")

        // render directional shadow map
        sceneView.m_render->m_csm->render(scene, scene->m_directionalLight, sceneView.m_cameraInfo);

        auto outDirectLighting = sceneView.m_render->directLighting();
        auto depth = sceneView.m_render->depth();
        auto albedo = sceneView.m_render->albedo();
        auto normal = sceneView.m_render->normal();
        auto metallicRoughness = sceneView.m_render->metallicRoughness();
        auto csm = sceneView.m_render->m_csm.get();

        if (scene != nullptr && outDirectLighting != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", SHADER_TEXT_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "SceneDirectLightingPS", SHADER_TEXT_PATH "scene_direct_lighting_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            const auto desc = outDirectLighting->getDesc();
            u32 width = desc.width, height = desc.height;
            const auto& viewState = sceneView.m_state;

            RenderingUtils::renderScreenPass(
                glm::uvec2(width, height),
                [outDirectLighting](RenderPass& rp) {
                    rp.setRenderTarget(outDirectLighting, 0);
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

    void SceneRenderer::tonemapping(GHTexture2D* dstTexture, GHTexture2D* srcTexture)
    {
        if (dstTexture != nullptr && srcTexture != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "ScreenPassVS", SHADER_TEXT_PATH "screen_pass_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "TonemappingPS", SHADER_TEXT_PATH "tonemapping_p.glsl");
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
    }
}