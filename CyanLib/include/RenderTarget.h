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
        TextureRenderable* getColorBuffer(u32 index);
        void setColorBuffer(TextureRenderable* texture, u32 index, u32 mip = 0u);
        void setDepthBuffer(TextureRenderable* texture);
        void clear(const std::initializer_list<RenderTargetDrawBuffer>& buffers, f32 clearDepth = 1.f);
        bool validate();

        u32 width, height;
        // gl frame buffer object
        GLuint fbo;
        // gl render buffer object
        GLuint rbo;
        // draw buffers
        Cyan::TextureRenderable* colorBuffers[8];
        Cyan::TextureRenderable* depthBuffer;
    };
}