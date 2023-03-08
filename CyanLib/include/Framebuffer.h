#pragma once

#include <vector>

#include <glew/glew.h>
#include <glm/glm.hpp>

#include "Texture.h"

namespace Cyan
{
    struct FramebufferDrawBuffer
    {
        i32 binding = -1;
        glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
    };

    struct RenderBuffer : public GpuResource
    {
        RenderBuffer(u32 inWidth, u32 inHeight)
            : width(inWidth), height(inHeight)
        {
            glCreateRenderbuffers(1, &glObject);
            glNamedRenderbufferStorage(getGpuResource(), GL_DEPTH24_STENCIL8, width, height);
        }

        ~RenderBuffer() 
        { 
            GLuint renderbuffers[] = { getGpuResource() };
            glDeleteRenderbuffers(1, renderbuffers);
        }

        u32 width, height;
    };

    struct Framebuffer : public GpuResource
    {
        ~Framebuffer()
        {
            GLuint framebuffers[] = { getGpuResource() };
            glDeleteFramebuffers(1, framebuffers);
        }

        static Framebuffer* create(u32 inWidth, u32 inHeight);

        GfxTexture* getColorBuffer(u32 index) { assert(index < kNumColorbufferBindings); return colorBuffers[index]; }
        GfxDepthTexture2D* getDepthBuffer() { return depthBuffer; }

        void setColorBuffer(GfxTexture2D* texture, u32 index, u32 mip = 0u);
        void setColorBuffer(TextureCube* texture, u32 index, u32 mip = 0u);
        void setDepthBuffer(GfxDepthTexture2D* depthTexture);
        void setDrawBuffers(const std::initializer_list<i32>& buffers);
        void clearDrawBuffer(i32 drawBufferIndex, glm::vec4 clearColor, bool clearDepth = true, f32 clearDepthValue = 1.f);
        void clearDepthBuffer(f32 clearDepthValue = 1.f);
        bool validate();
        // reset states of this framebuffer object
        void reset();

        u32 width, height;
        static constexpr u32 kNumColorbufferBindings = 8;
        // framebuffer doesn't own any of its color attachments or depth attachments, it's basically just a container
        GfxTexture* colorBuffers[kNumColorbufferBindings] = { 0 };
        GfxDepthTexture2D* depthBuffer = nullptr;
    private:
        Framebuffer(u32 inWidth, u32 inHeight) 
            : width(inWidth), height(inHeight)
        { 
            glCreateFramebuffers(1, &glObject);
#if 0
            if (inDepthBuffer)
            {
                setDepthBuffer(inDepthBuffer);
            }
            else
            {
                renderBuffer = std::make_shared<RenderBuffer>(width, height);
                glNamedFramebufferRenderbuffer(getGpuResource(), GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBuffer->getGpuResource());
            }
#endif
        }
    };
}