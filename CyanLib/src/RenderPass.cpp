#include "RenderPass.h"

namespace Cyan
{
    glm::uvec2 getFramebufferSize(GfxTexture2D* texture) { return glm::uvec2(texture->width, texture->height); }
    glm::uvec2 getFramebufferSize(GfxDepthTexture2D* texture) { return glm::uvec2(texture->width, texture->height); }
    glm::uvec2 getFramebufferSize(TextureCube* texture) { return glm::uvec2(texture->resolution, texture->resolution); }

    void RenderPass::setRenderTarget(const RenderTarget& inRenderTarget, u32 binding)
    {
        assert(binding < 8);
        assert(inRenderTarget.width == framebufferSize.x && inRenderTarget.height == framebufferSize.y);
        renderTargets[binding] = inRenderTarget;
    }

    void RenderPass::setDepthBuffer(GfxDepthTexture2D* inDepthTexture)
    {
        assert(inDepthTexture->width == framebufferSize.x && inDepthTexture->height == framebufferSize.y);
        depthBuffer = inDepthTexture;
    }

    bool RenderPass::hasAnyRenderTargetBound()
    {
        bool bHasAnyRenderTargetBound = false;
        for (i32 i = 0; i < 8; ++i)
        {
            bHasAnyRenderTargetBound |= (renderTargets[i].texture != nullptr);
        }
        bHasAnyRenderTargetBound |= (depthBuffer != nullptr);
        return bHasAnyRenderTargetBound;
    }

    Framebuffer* RenderPass::createFramebuffer(GfxContext* ctx)
    {
        Framebuffer* outFramebuffer = nullptr;

        if (hasAnyRenderTargetBound()) 
        {
            outFramebuffer = ctx->createFramebuffer(framebufferSize.x, framebufferSize.y);
        }

        if (outFramebuffer != nullptr)
        {
            u32 colorBufferBinding = 0;
            i32 drawBuffers[8] = { -1, -1, -1, -1, -1, -1, -1, -1};
            // setup color buffers
            for (i32 i = 0; i < 8; ++i)
            {
                RenderTarget renderTarget = renderTargets[i];
                if (renderTarget.texture)
                {
                    if (auto textureCube = dynamic_cast<TextureCube*>(renderTarget.texture))
                    {
                        outFramebuffer->setColorBuffer(textureCube, colorBufferBinding, renderTarget.layer, renderTarget.mip);
                    }
                    else if (auto texture2D = dynamic_cast<GfxTexture2D*>(renderTarget.texture))
                    {
                        outFramebuffer->setColorBuffer(texture2D, colorBufferBinding, renderTarget.mip);
                    }
                    else
                    {
                        assert(0);
                    }

                    drawBuffers[i] = colorBufferBinding;
                    colorBufferBinding++;
                }

            }
            outFramebuffer->setDrawBuffers(drawBuffers, 8);
            for (i32 i = 0; i < 8; ++i)
            {
                if (drawBuffers[i] >= 0) 
                {
                    outFramebuffer->clearDrawBuffer(i, renderTargets[i].clearColor);
                }
            }
            // setup depth buffer
            if (depthBuffer != nullptr)
            {
                outFramebuffer->setDepthBuffer(depthBuffer);
                outFramebuffer->clearDepthBuffer();
            }
        }

        return outFramebuffer;
    }

    void RenderPass::render(GfxContext* ctx)
    {
        // setup framebuffer according to render targets
        Framebuffer* framebuffer = createFramebuffer(ctx);
        ctx->setFramebuffer(framebuffer);
        ctx->setViewport(viewport);

        // setup gfx pipeline state
        ctx->setDepthControl(gfxPipelineState.depth);
        ctx->setPrimitiveType(gfxPipelineState.primitiveMode);

        // setup shader
        ctx->setPixelPipeline(pipeline, shaderSetupLambda);

        // draw
        drawLambda(ctx);

        // clean up states
        ctx->reset();
    }
}