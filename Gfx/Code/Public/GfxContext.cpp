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

    void GfxContext::enableBackfaceCulling()
    {
        m_GHCtx->enableBackfaceCulling();
    }

    void GfxContext::disableBackfaceCulling()
    {
        m_GHCtx->disableBackfaceCulling();
    }

    void GfxContext::enableBlending()
    {
        m_GHCtx->enableBlending();
    }

    void GfxContext::disableBlending()
    {
        m_GHCtx->disableBlending();
    }

    void GfxContext::setBlendingMode(const BlendingMode& bm)
    {
        m_GHCtx->setBlendingMode(bm);
    }

    void GfxContext::pushGpuDebugMarker(const char* markerName)
    {
        m_GHCtx->pushGpuDebugMarker(markerName);
    }

    void GfxContext::popGpuDebugMarker()
    {
        m_GHCtx->popGpuDebugMarker();
    }

    void GfxContext::setGeometryMode(const GeometryMode& mode)
    {
        m_GHCtx->setGeometryMode(mode);
    }

    void GfxContext::drawArrays(u32 numVertices)
    {
        m_GHCtx->drawArrays(numVertices);
    }

    void GfxContext::dispatchCompute(ComputePipeline* cp, i32 threadGroupSizeX, i32 threadGroupSizeY, i32 threadGroupSizeZ)
    {
        m_GHCtx->dispatchCompute(threadGroupSizeX, threadGroupSizeY, threadGroupSizeZ);
    }

    std::unique_ptr<GfxStaticSubMesh> GfxContext::createGfxStaticSubMesh(Geometry* geometry)
    {
        return m_GHCtx->createGfxStaticSubMesh(geometry);
    }
}

