#pragma once

#include "GHBuffer.h"
#include "GHTexture.h"
#include "GHShader.h"
#include "GHPipeline.h"
#include "GHFramebuffer.h"

namespace Cyan
{
    enum GfxHardwareAPI
    {
        OpenGL = 0,
        Vulkan,
        Count
    };

    static GfxHardwareAPI getActiveGfxHardwareAPI();

    class GfxHardwareContext
    {
    public:
        static GfxHardwareContext* create();
        static GfxHardwareContext* get();

        virtual ~GfxHardwareContext() { }
        // buffers
        virtual GHVertexBuffer* createVertexBuffer() = 0;
        virtual GHIndexBuffer* createIndexBuffer() = 0;
        // shaders
        virtual GHShader* createVertexShader(const char* text) = 0;
        virtual GHShader* createPixelShader(const char* text) = 0;
        virtual GHShader* createComputeShader(const char* text) = 0;
        virtual GHGfxPipeline* createGfxPipeline(std::shared_ptr<GHShader> vs, std::shared_ptr<GHShader> ps) = 0;
        // framebuffers
        virtual GHFramebuffer* createFramebuffer(u32 width, u32 height) = 0;
        virtual void setViewport(i32 x, i32 y, i32 width, i32 height) = 0;
    protected:
        GfxHardwareContext() { }
    private:
        static GfxHardwareContext* s_instance;
    };
}
