#pragma once

#include <vector>

#include "glew.h"

#include "Texture.h"

namespace Cyan
{
    struct RenderTarget
    {
        void setColorBuffer(Texture* texture, u32 index);
        void attachColorBuffer(Texture* _texture, u32 mip=0);
        void setDrawBuffer(u32 _bufferIdx);
        Texture* getColorBuffer(u32 index);
        bool validate();

        u32 m_width;
        u32 m_height;
        u32 m_numColorBuffers;
        GLuint m_renderBuffer;
        GLuint m_frameBuffer;
        std::vector<Cyan::Texture*> m_colorBuffers;
    };
}