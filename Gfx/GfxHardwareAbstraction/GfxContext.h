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

    class GfxContext
    {
    public:
        ~GfxContext();
        static GfxContext* get();

        void setFramebuffer(GHFramebuffer* framebuffer);
        void unsetFramebuffer();
        void setViewport(i32 x, i32 y, i32 width, i32 height);
        void unsetViewport();

    private:
        GfxContext();

        GfxHardwareContext* m_GHCtx = nullptr;
        GfxPipeline* m_gfxPipeline = nullptr;
        GHFramebuffer* m_framebuffer = nullptr;
        Viewport m_viewport = { };

        static GfxContext* s_instance;
    };
}