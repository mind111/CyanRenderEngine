#pragma once

#include <vector>

#include "glew.h"

#include "Texture.h"

namespace Cyan
{
    // TODO: refactor this; 
    struct RenderTarget
    {
        void setColorBuffer(Texture* texture, u32 index);
        void attachColorBuffer(Texture* _texture, u32 mip=0);
        void attachTexture(Texture* texture, u32 index, u32 faceIndex = -1);
        void setDepthBuffer(Texture* texture);
        void setDrawBuffer(u32 bufferIdx);
        void bindDrawBuffers();
        void setDrawBuffers(i32* buffers, u32 numBuffers);
        Texture* getColorBuffer(u32 index);
        bool validate();

        u32 m_width;
        u32 m_height;
        u32 m_numColorBuffers;
        u32 m_numDepthBuffers;
        GLuint m_renderBuffer;
        GLuint m_frameBuffer;
        std::vector<Cyan::Texture*> m_colorBuffers;
        // draw buffer binding
        u32 m_numDrawBuffers;
        i32 m_drawBuffers[8];
        Cyan::Texture* m_textures[8];
        Cyan::Texture* m_depthBuffer;
    };
}