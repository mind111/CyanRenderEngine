#pragma once

#include <vector>

#include "glew.h"
#include "glm.hpp"

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
        ITextureRenderable* getColorBuffer(u32 index);
        void setColorBuffer(Texture2DRenderable* texture, u32 index, u32 mip = 0u);
        void setColorBuffer(TextureCubeRenderable* texture, u32 index, u32 mip = 0u);
        void setDepthBuffer(DepthTexture* texture);
        void clear(const std::initializer_list<RenderTargetDrawBuffer>& buffers, f32 clearDepth = 1.f);
        bool validate();

        static RenderTarget* defaultRenderTarget;
        u32 width, height;
        Cyan::ITextureRenderable* colorBuffers[8] = { 0 };
        Cyan::DepthTexture* depthBuffer = nullptr;
        GLuint fbo = -1;
        GLuint rbo = -1;
    };
}