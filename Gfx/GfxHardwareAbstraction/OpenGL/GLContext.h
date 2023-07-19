#pragma once

#include "GfxHardwareContext.h"

namespace Cyan
{
    class GLGHContext : public GfxHardwareContext
    {
    public:
        GLGHContext();
        virtual ~GLGHContext();

        // buffers
        virtual GHVertexBuffer* createVertexBuffer() override;
        virtual GHIndexBuffer* createIndexBuffer() override;
        // shaders
        virtual GHShader* createVertexShader(const char* text) override;
        virtual GHShader* createPixelShader(const char* text) override;
        virtual GHShader* createComputeShader(const char* text) override;
        virtual GHGfxPipeline* createGfxPipeline(std::shared_ptr<GHShader> vs, std::shared_ptr<GHShader> ps) override;
        // framebuffers
        virtual GHFramebuffer* createFramebuffer(u32 width, u32 height) override;
        virtual void setViewport(i32 x, i32 y, i32 width, i32 height) override;
    private:
    };
}