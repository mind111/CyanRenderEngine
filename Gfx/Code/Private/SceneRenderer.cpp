#include "SceneRender.h"
#include "SceneRenderer.h"
#include "RenderPass.h"
#include "Shader.h"
#include "Scene.h"
#include "StaticMesh.h"

namespace Cyan
{
    SceneRenderer::SceneRenderer()
    {

    }

    SceneRenderer::~SceneRenderer() { }

    void SceneRenderer::render(ISceneRender* outRender, IScene* scene, const SceneViewState& viewInfo)
    {
        // render depth prepass
    }

    static void setShaderSceneViewInfo(GfxPipeline* gfxp, const SceneViewState& viewState)
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

    void SceneRenderer::renderSceneDepth(GHDepthTexture* outDepthTex, Scene* scene, const SceneViewState& viewState)
    {
        if (outDepthTex != nullptr && scene != nullptr)
        {
            bool found = false;
            auto vs = ShaderManager::findOrCreateShader<VertexShader>(found, "StaticMeshVS", "static_mesh_v.glsl");
            auto ps = ShaderManager::findOrCreateShader<PixelShader>(found, "DepthPS", "depth_p.glsl");
            auto gfxp = ShaderManager::findOrCreateGfxPipeline(found, vs, ps);

            const auto desc = outDepthTex->getDesc();
            u32 width = desc.width, height = desc.height;
            RenderPass rp(width, height);
            rp.setDepthTarget(outDepthTex);
            rp.setRenderFunc([gfxp, scene, viewState](GfxContext* gfxc) {
                gfxp->bind();
                setShaderSceneViewInfo(gfxp.get(), viewState);
                for (auto instance : scene->m_staticMeshInstances)
                {
                    gfxp->setUniform("localToWorld", instance->getLocalToWorldMatrix());
                    auto mesh = instance->getParentMesh();
                    for (u32 i = 0; i < mesh->numSubMeshes(); ++i)
                    {
                        auto sm = (*mesh)[i];
                        sm->gfxProxy->draw();
                    }
                }
                gfxp->unbind();
            });
            rp.render(GfxContext::get());
        }
    }
}