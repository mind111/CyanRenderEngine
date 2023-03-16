#pragma once

#include "GfxContext.h"

namespace Cyan
{
    glm::uvec2 getFramebufferSize(GfxTexture2D* texture);
    glm::uvec2 getFramebufferSize(GfxDepthTexture2D* texture);
    glm::uvec2 getFramebufferSize(GfxTextureCube* texture);

    /**
     * RenderPass encapsulates the the rendering work that shares the same set of render targets, depth buffer (same framebuffer setup) and graphics pipeline states
     */
    struct RenderPass
    {
        RenderPass(u32 framebufferWidth, u32 framebufferHeight)
            : framebufferSize(framebufferWidth, framebufferHeight)
        {

        }

        void render(GfxContext* ctx);

        void setRenderTarget(const RenderTarget& inRenderTarget, u32 binding, bool bClear = true);
        void setDepthBuffer(GfxDepthTexture2D* inDepthTexture, bool bClear = true);

        glm::uvec2 framebufferSize = glm::uvec2(0u);
        Viewport viewport = { };
        // todo: should also allow pipeline to be changed during the execution of drawLambda
        // PixelPipeline* pipeline = nullptr;
        GfxPipelineState gfxPipelineState = { };
        std::function<void(GfxContext*)> drawLambda = [](GfxContext* ctx) { };
    private:
        Framebuffer* createFramebuffer(GfxContext* ctx);
        bool hasAnyRenderTargetBound();

        RenderTarget renderTargets[8];
        bool bPendingClearColor[8] = { false, false, false, false, false, false, false, false };
        GfxDepthTexture2D* depthBuffer = nullptr;
        bool bPendingClearDepth = false;
    };
}
