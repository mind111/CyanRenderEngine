#include "SceneRender.h"
#include "SceneRenderer.h"
#include "RenderPass.h"
#include "Shader.h"

namespace Cyan
{
    SceneRenderer::SceneRenderer()
    {

    }

    SceneRenderer::~SceneRenderer() { }

    void SceneRenderer::render(ISceneRender* outRender, IScene* scene, const SceneViewInfo& viewInfo)
    {
        // render depth prepass
    }

    /**
        if (outDepthBuffer != nullptr)
        {
            CreateVS(vs, "StaticMeshVS", SHADER_SOURCE_PATH "static_mesh_v.glsl");
            CreatePS(ps, "DepthOnlyPS", SHADER_SOURCE_PATH "depth_only_p.glsl");
            CreatePixelPipeline(pipeline, "SceneDepthPrepass", vs, ps);

            RenderPass pass(outDepthBuffer->width, outDepthBuffer->height);
            pass.viewport = { 0, 0, outDepthBuffer->width, outDepthBuffer->height };
            pass.setDepthBuffer(outDepthBuffer);
            pass.drawLambda = [&scene, pipeline, viewParameters](GfxContext* ctx) { 
                pipeline->bind(ctx);
                viewParameters.setShaderParameters(pipeline);
                for (auto instance : scene->m_staticMeshInstances)
                {
                    pipeline->setUniform("localToWorld", instance->localToWorld.toMatrix());
                    auto mesh = instance->parent;
                    for (i32 i = 0; i < mesh->getNumSubmeshes(); ++i)
                    {
                        auto sm = (*mesh)[i];
                        ctx->setVertexArray(sm->va.get());
                        ctx->drawIndex(sm->numIndices());
                    }
                }
                pipeline->unbind(ctx);
            };
            pass.render(m_ctx);
        }
     */

    static void setShaderSceneViewInfo(GfxPipeline* gfxp, const SceneViewInfo& viewInfo)
    {
        gfxp->setUniform("viewParameters.renderResolution", viewInfo.resolution);
        gfxp->setUniform("viewParameters.aspectRatio", viewInfo.aspectRatio);
        gfxp->setUniform("viewParameters.viewMatrix", viewInfo.viewMatrix);
        gfxp->setUniform("viewParameters.prevFrameViewMatrix", viewInfo.prevFrameViewMatrix);
        gfxp->setUniform("viewParameters.projectionMatrix", viewInfo.projectionMatrix);
        gfxp->setUniform("viewParameters.prevFrameProjectionMatrix", viewInfo.prevFrameProjectionMatrix);
        gfxp->setUniform("viewParameters.cameraPosition", viewInfo.cameraPosition);
        gfxp->setUniform("viewParameters.prevFrameCameraPosition", viewInfo.prevFrameCameraPosition);
        gfxp->setUniform("viewParameters.cameraRight", viewInfo.cameraRight);
        gfxp->setUniform("viewParameters.cameraForward", viewInfo.cameraForward);
        gfxp->setUniform("viewParameters.cameraUp", viewInfo.cameraUp);
        gfxp->setUniform("viewParameters.frameCount", viewInfo.frameCount);
        gfxp->setUniform("viewParameters.elapsedTime", viewInfo.elapsedTime);
        gfxp->setUniform("viewParameters.deltaTime", viewInfo.deltaTime);
    }

    // todo: implement this
    // 1. get Shader to work
    // 2. get Texture2D, and DepthTexture2D to work
    void SceneRenderer::renderSceneDepth(GHDepthTexture* outDepthTex, Scene* scene, const SceneViewInfo& viewInfo)
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
            rp.setRenderFunc([gfxp, scene, viewInfo](GfxContext* gfxc) {
                gfxp->bind();
                setShaderSceneViewInfo(gfxp.get(), viewInfo);
#if 0
                for (auto instance : scene->m_staticMeshInstances)
                {
                    pipeline->setUniform("localToWorld", instance->localToWorld.toMatrix());
                    auto mesh = instance->parent;
                    for (i32 i = 0; i < mesh->getNumSubmeshes(); ++i)
                    {
                        auto sm = (*mesh)[i];
                        ctx->setVertexArray(sm->va.get());
                        ctx->drawIndex(sm->numIndices());
                    }
                }
#endif
                gfxp->unbind();
            });
            rp.render(GfxContext::get());
        }
    }
}