#pragma once

#include "GfxContext.h"

namespace Cyan
{
    glm::uvec2 getFramebufferSize(GfxTexture2D* texture);
    glm::uvec2 getFramebufferSize(GfxDepthTexture2D* texture);
    glm::uvec2 getFramebufferSize(TextureCube* texture);

    struct RenderPass
    {
        RenderPass(u32 framebufferWidth, u32 framebufferHeight)
            : framebufferSize(framebufferWidth, framebufferHeight)
        {

        }

        void render(GfxContext* ctx);

        void setRenderTarget(const RenderTarget& inRenderTarget, u32 binding);
        void setDepthBuffer(GfxDepthTexture2D* inDepthTexture);

        glm::uvec2 framebufferSize = glm::uvec2(0u);
        Viewport viewport;
        PixelPipeline* pipeline = nullptr;
        GfxPipelineState gfxPipelineState = { };
        std::function<void(VertexShader*, PixelShader*)> shaderSetupLambda = [](VertexShader* vs, PixelShader* ps) { };
        std::function<void(GfxContext*)> drawLambda = [](GfxContext* ctx) { };
    private:
        Framebuffer* createFramebuffer(GfxContext* ctx);
        bool hasAnyRenderTargetBound();

        RenderTarget renderTargets[8];
        GfxDepthTexture2D* depthBuffer = nullptr;
    };
}
