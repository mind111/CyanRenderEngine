#pragma once

#include "GfxHardwareContext.h"

namespace Cyan
{
    class GLGHContext : public GfxHardwareContext
    {
    public:
        GLGHContext();
        virtual ~GLGHContext();

        virtual GHVertexBuffer* createVertexBuffer() override;
        virtual GHIndexBuffer* createIndexBuffer() override;
        virtual GHVertexShader* createVertexShader(const char* text) override;
        virtual GHPixelShader* createPixelShader(const char* text) override;
        virtual GHComputeShader* createComputeShader(const char* text) override;
        virtual GHGfxPipeline* createGfxPipeline(const char* vsText, const char* psText) override;
    private:
    };
}