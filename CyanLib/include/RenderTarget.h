#pragma once

#include <vector>

#include <glew/glew.h>
#include <glm/glm.hpp>

#include "Texture.h"

namespace Cyan
{
    struct RenderTargetDrawBuffer
    {
        i32 binding = -1;
        glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
    };

    struct RenderTarget
    {
        ~RenderTarget()
        {
            if (fbo >= 0)
            {
                glDeleteFramebuffers(1, &fbo);
            }
            if (rbo >= 0)
            {
                glDeleteRenderbuffers(1, &rbo);
            }
        }

        static RenderTarget* getDefaultRenderTarget(u32 width, u32 height);
        ITexture* getColorBuffer(u32 index);
        void setColorBuffer(Texture2D* texture, u32 index, u32 mip = 0u);
        void setColorBuffer(TextureCube* texture, u32 index, u32 mip = 0u);
        void setDrawBuffers(const std::initializer_list<i32>& buffers);
        void setDepthBuffer(DepthTexture2D* texture);
        void clear(const std::initializer_list<RenderTargetDrawBuffer>& buffers, f32 clearDepthBuffer = 1.f);
        void clearDrawBuffer(i32 drawBufferIndex, glm::vec4 clearColor, bool clearDepth = true, f32 clearDepthValue = 1.f);
        void clearDepthBuffer(f32 clearDepthValue = 1.f);
        bool validate();

        static RenderTarget* defaultRenderTarget;
        u32 width, height;
        Cyan::ITexture* colorBuffers[8] = { 0 };
        Cyan::DepthTexture2D* depthBuffer = nullptr;
        GLuint fbo = -1;
        GLuint rbo = -1;
    };
}