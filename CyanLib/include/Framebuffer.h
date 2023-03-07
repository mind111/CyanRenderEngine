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
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, getGpuResource());
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
        void setDrawBuffers(const std::initializer_list<i32>& buffers);
        void setDepthBuffer(GfxDepthTexture2D* depthTexture);
        void clearDrawBuffer(i32 drawBufferIndex, glm::vec4 clearColor, bool clearDepth = true, f32 clearDepthValue = 1.f);
        void clearDepthBuffer(f32 clearDepthValue = 1.f);
        bool validate();

        u32 width, height;
        GfxDepthTexture2D* depthBuffer = nullptr;
        std::shared_ptr<RenderBuffer> renderBuffer = nullptr;
        static constexpr u32 kNumColorbufferBindings = 8;
        GfxTexture* colorBuffers[kNumColorbufferBindings] = { 0 };

    private:
        Framebuffer(u32 inWidth, u32 inHeight, GfxDepthTexture2D* inDepthBuffer = nullptr) 
            : width(inWidth), height(inHeight), depthBuffer(inDepthBuffer)
        { 
            glCreateFramebuffers(1, &glObject);
            if (inDepthBuffer)
            {
                setDepthBuffer(inDepthBuffer);
            }
            else
            {
                renderBuffer = std::make_shared<RenderBuffer>(width, height);
            }
        }
    };
}