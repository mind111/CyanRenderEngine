#pragma once

#include "GHBuffer.h"
#include "GHTexture.h"
#include "GHShader.h"
#include "GHPipeline.h"

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

        virtual ~GfxHardwareContext() { }
        virtual GHVertexBuffer* createVertexBuffer() = 0;
        virtual GHIndexBuffer* createIndexBuffer() = 0;

        virtual GHVertexShader* createVertexShader(const char* text) = 0;
        virtual GHPixelShader* createPixelShader(const char* text) = 0;
        virtual GHComputeShader* createComputeShader(const char* text) = 0;
        virtual GHGfxPipeline* createGfxPipeline(const char* vsText, const char* psText) = 0;
    protected:
        GfxHardwareContext() { }
    private:
        static GfxHardwareContext* s_instance;
    };
}
