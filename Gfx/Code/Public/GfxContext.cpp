#include "GfxContext.h"

namespace Cyan
{
    GfxContext* GfxContext::s_instance = nullptr;
    GfxContext::GfxContext()
    {
        m_GHCtx = GfxHardwareContext::create();
    }

    GfxContext::~GfxContext()
    {

    }

    GfxContext* GfxContext::get()
    {
        static std::unique_ptr<GfxContext> s_gfxc(new GfxContext());
        if (s_instance == nullptr)
        {
            s_instance = s_gfxc.get();
        }
        return s_instance;
    }

    void GfxContext::initialize()
    {
        m_GHCtx->initialize();
    }


    void GfxContext::deinitialize()
    {
        m_GHCtx->deinitialize();
    }

    void GfxContext::setFramebuffer(GHFramebuffer* framebuffer)
    {
        if (m_framebuffer != nullptr)
        {
            m_framebuffer->unbind();
            m_framebuffer = nullptr;
        }
        if (framebuffer != nullptr)
        {
            framebuffer->bind();
        }
        m_framebuffer = framebuffer;
    }

    void GfxContext::unsetFramebuffer()
    {
        if (m_framebuffer != nullptr)
        {
            m_framebuffer->unbind();
        }
        m_framebuffer = nullptr;
    }

    void GfxContext::setViewport(i32 x, i32 y, i32 width, i32 height)
    {
        m_GHCtx->setViewport(x, y, width, height);
        m_viewport = { x, y, width, height };
    }

    void GfxContext::unsetViewport()
    {
        m_GHCtx->setViewport(0, 0, 0, 0);
        m_viewport = { };
    }

    void GfxContext::enableDepthTest()
    {
        bDepthTest = true;
        m_GHCtx->enableDepthTest();
    }

    void GfxContext::disableDepthTest()
    {
        bDepthTest = false;
        m_GHCtx->disableDepthTest();
    }

    std::unique_ptr<GfxStaticSubMesh> GfxContext::createGfxStaticSubMesh(Geometry* geometry)
    {
        return m_GHCtx->createGfxStaticSubMesh(geometry);
    }
}

