#pragma once

#include <vector>

#include "glew.h"

#include "Texture.h"

namespace Cyan
{
    struct RenderTarget
    {
        void attachColorBuffer(Texture* _texture, u32 mip=0);
        void setDrawBuffer(u32 _bufferIdx);
        bool validate();

        u32 m_width;
        u32 m_height;
        u32 m_numColorBuffers;
        GLuint m_renderBuffer;
        GLuint m_frameBuffer;
        std::vector<Cyan::Texture*> m_colorBuffers;
    };
}