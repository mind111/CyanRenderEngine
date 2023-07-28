#pragma once

#include "Shader.h"
#include "GHFramebuffer.h"
#include "GfxHardwareContext.h"

namespace Cyan
{
    struct Viewport
    {
        i32 x = 0, y = 0;
        i32 width = 0, height = 0;
    };

    struct Geometry;

    class GfxContext
    {
    public:
        ~GfxContext();
        static GfxContext* get();

        void initialize();
        void deinitialize();

        void setFramebuffer(GHFramebuffer* framebuffer);
        void unsetFramebuffer();
        void setViewport(i32 x, i32 y, i32 width, i32 height);
        void unsetViewport();
        void enableDepthTest();
        void disableDepthTest();
        std::unique_ptr<GfxStaticSubMesh> createGfxStaticSubMesh(Geometry* geometry);

    private:
        GfxContext();

        GfxHardwareContext* m_GHCtx = nullptr;
        GfxPipeline* m_gfxPipeline = nullptr;
        GHFramebuffer* m_framebuffer = nullptr;
        Viewport m_viewport = { };

        bool bDepthTest = false;

        static GfxContext* s_instance;
    };
}