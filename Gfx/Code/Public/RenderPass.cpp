#include "RenderPass.h"
#include "GfxHardwareAbstraction/GHInterface/GfxHardwareContext.h"

namespace Cyan
{
    RenderPass::RenderPass(u32 renderWidth, u32 renderHeight)
        : m_viewport{ 0, 0, (i32)renderWidth, (i32)renderHeight }
        , m_renderWidth(renderWidth)
        , m_renderHeight(renderHeight)
        , bDepthTest(false)
        , m_renderFunc(RenderFunc())
    {

    }

    RenderPass::~RenderPass()
    {

    }

    GHFramebuffer* RenderPass::createFramebuffer(const RenderPass& pass)
    {
        /**
         * framebuffer cache
         */
        static std::unordered_map<std::string, std::unique_ptr<GHFramebuffer>> s_framebufferMap;

        auto buildKey = [](u32 renderWidth, u32 renderHeight) -> std::string {
            std::string key = std::to_string(renderWidth) + "x" + std::to_string(renderHeight);
            return key;
        };

        auto key = buildKey(pass.m_renderWidth, pass.m_renderHeight);
        auto cached = s_framebufferMap.find(key);
        if (cached != s_framebufferMap.end())
        {
            return cached->second.get();
        }

        // create a new framebuffer
        GHFramebuffer* framebuffer = GfxHardwareContext::get()->createFramebuffer(pass.m_renderWidth, pass.m_renderHeight);
        s_framebufferMap.insert({ key, std::unique_ptr<GHFramebuffer>(framebuffer) });
        return framebuffer;
    }

    void RenderPass::setupFramebuffer(GHFramebuffer* framebuffer)
    {
        framebuffer->setDrawBuffers(m_drawBufferBindings);

        for (i32 i = 0; i < FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS; ++i)
        {
            if (m_renderTargets[i].isBound())
            {
                switch (m_renderTargets[i].type)
                {
                case RenderTarget::Type::k2D: {
                    auto tex2D = static_cast<GHTexture2D*>(m_renderTargets[i].colorTexture);
                    framebuffer->bindColorBuffer(tex2D, i, m_renderTargets[i].mipLevel);
                    break;
                }
                case RenderTarget::Type::kCube: {
                    auto texCube = static_cast<GHTextureCube*>(m_renderTargets[i].colorTexture);
                    framebuffer->bindColorBuffer(texCube, m_renderTargets[i].cubeFaceIndex, i, m_renderTargets[i].mipLevel);
                    break;
                    break;
                }
                default: assert(0); break;
                }
                if (m_renderTargets[i].bNeedsClear)
                {
                    framebuffer->clearColorBuffer(i, m_renderTargets[i].clearColor);
                }
            }
        }

        if (m_depthTarget.isBound())
        {
            framebuffer->bindDepthBuffer(m_depthTarget.depthTexture);
            if (m_depthTarget.bNeedsClear)
            {
                framebuffer->clearDepthBuffer(m_depthTarget.clearValue);
            }
        }
    }

    void RenderPass::cleanupFramebuffer(GHFramebuffer* framebuffer)
    {
        for (i32 i = 0; i < FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS; ++i)
        {
            if (m_renderTargets[i].isBound())
            {
                framebuffer->unbindColorBuffer(i);
            }
        }
        framebuffer->unsetDrawBuffers();

        if (m_depthTarget.isBound())
        {
            framebuffer->unbindDepthBuffer();
        }
    }

    void RenderPass::setRenderTarget(const RenderTarget& renderTarget, u32 slot)
    {
        assert(slot < FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS);
        assert(renderTarget.width() == m_renderWidth && renderTarget.height() == m_renderHeight);
        assert(!m_renderTargets[slot].isBound());

        m_renderTargets[slot] = renderTarget;
        /**
         * Note: for now, enforce that a color buffer n can only be bind to draw buffer slot n.
         */
        ColorBuffer cb = (ColorBuffer)((i32)ColorBuffer::Color0 + (i32)slot);
        m_drawBufferBindings.bind(cb, slot);
        bHasAnyTargetBound |= true;
    }

    void RenderPass::setDepthTarget(const DepthTarget& depthTarget)
    {
        assert(depthTarget.width() == m_renderWidth && depthTarget.height() == m_renderHeight);

        m_depthTarget = depthTarget;
        bHasAnyTargetBound |= true;
    }

    void RenderPass::setViewport(i32 x, i32 y, i32 width, i32 height)
    {
        m_viewport = { x, y, width, height };
    }

    void RenderPass::setRenderFunc(const RenderFunc& renderFunc)
    {
        m_renderFunc = renderFunc;
    }

    void RenderPass::render(GfxContext* ctx)
    {
        /*
        // setup framebuffer according to render targets
        Framebuffer* framebuffer = createFramebuffer(ctx);
        ctx->setFramebuffer(framebuffer);
        ctx->setViewport(viewport);

        // setup gfx pipeline state
        ctx->setDepthControl(gfxPipelineState.depth);
        ctx->setPrimitiveType(gfxPipelineState.primitiveMode);

        // draw
        drawLambda(ctx);

        // clean up states
        ctx->reset();
        */
        if (bHasAnyTargetBound)
        {
            GHFramebuffer* framebuffer = createFramebuffer(*this);

            // setup
            setupFramebuffer(framebuffer);
            ctx->setFramebuffer(framebuffer);
            ctx->setViewport(m_viewport.x, m_viewport.y, m_viewport.width, m_viewport.height);

            bDepthTest ? ctx->enableDepthTest() : ctx->disableDepthTest();
            m_renderFunc(ctx);

            // clean up
            ctx->unsetViewport();
            ctx->unsetFramebuffer();
            cleanupFramebuffer(framebuffer);
        }
        else
        {
            if (bRenderToDefaultTarget)
            {
                ctx->setViewport(m_viewport.x, m_viewport.y, m_viewport.width, m_viewport.height);
                bDepthTest ? ctx->enableDepthTest() : ctx->disableDepthTest();
                m_renderFunc(ctx);
                ctx->unsetViewport();
            }
            else
            {
                UNREACHABLE_ERROR()
            }
        }
    }
}