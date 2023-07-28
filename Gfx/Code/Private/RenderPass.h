#pragma once

#include "Core.h"
#include "Shader.h"
#include "GHTexture.h"
#include "GHFramebuffer.h"
#include "GfxContext.h"
#include "MathLibrary.h"

namespace Cyan
{
    struct RenderTarget
    {
        u32 width() const
        { 
            const auto& desc = colorTexture->getDesc();
            return desc.width;
        }

        u32 height() const
        {
            const auto& desc = colorTexture->getDesc();
            return desc.height;
        }

        bool isBound() { return colorTexture != nullptr; }

        GHTexture2D* colorTexture = nullptr;
        u32 mipLevel = 0;
        bool bNeedsClear = true;
        glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
    };

    struct DepthTarget
    {
        DepthTarget() = default;
        DepthTarget(GHDepthTexture* inDepthTexture, bool shouldClearDepth = true, f32 depthClearValue = 1.f)
            : depthTexture(inDepthTexture), bNeedsClear(shouldClearDepth), clearValue(depthClearValue)
        {

        }

        u32 width() const
        { 
            const auto& desc = depthTexture->getDesc();
            return desc.width;
        }

        u32 height() const
        {
            const auto& desc = depthTexture->getDesc();
            return desc.height;
        }

        bool isBound() { return depthTexture != nullptr; }

        GHDepthTexture* depthTexture = nullptr;
        bool bNeedsClear = true;
        f32 clearValue = 1.f;
    };

    class RenderPass
    {
    public:
        using RenderFunc = std::function<void(GfxContext*)>;

        RenderPass() = delete;
        RenderPass(u32 renderWidth, u32 renderHeight);
        ~RenderPass();

        void enableRenderToDefaultTarget() { bRenderToDefaultTarget = true; }
        void disableRenderToDefaultTarget() { bRenderToDefaultTarget = false; }

        void setRenderTarget(const RenderTarget& renderTarget, u32 slot);
        void setDepthTarget(const DepthTarget& depthTarget);
        void setViewport(i32 x, i32 y, i32 width, i32 height);

        void enableDepthTest() { bDepthTest = true; }
        void disableDepthTest() { bDepthTest = false; }

        void setRenderFunc(const RenderFunc& renderFunc);
        void render(GfxContext* ctx);
    private:
        static GHFramebuffer* createFramebuffer(const RenderPass& pass);
        void setupFramebuffer(GHFramebuffer* framebuffer);
        void cleanupFramebuffer(GHFramebuffer* framebuffer);

        bool bRenderToDefaultTarget = false;
        bool bHasAnyTargetBound = false;

        std::array<RenderTarget, FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS> m_renderTargets;
        DrawBufferBindings m_drawBufferBindings;
        DepthTarget m_depthTarget;
        Viewport m_viewport;
        u32 m_renderWidth = 0, m_renderHeight = 0;
        bool bDepthTest = false;

        RenderFunc m_renderFunc;
    };

#define IM_DISPATCH_GFX_PASS()
}
