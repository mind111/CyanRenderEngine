#include "SceneRender.h"
#include "SceneRenderer.h"
#include "RenderPass.h"
#include "Shader.h"
#include "Scene.h"
#include "GfxStaticMesh.h"

namespace Cyan
{
    SceneRenderer::SceneRenderer()
    {

    }

    SceneRenderer::~SceneRenderer() { }

    void SceneRenderer::render(Scene* scene, SceneView& sceneView)
    {
        // render depth prepass
        GHDepthTexture* sceneDepthTex = sceneView.m_render->depth();
        renderSceneDepth(sceneDepthTex, scene, sceneView.m_state);

        // render gbuffer

        // render lighting

        // render postprocessing
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
                setShaderSceneViewInfo(gfxp.get(), viewState);
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
}