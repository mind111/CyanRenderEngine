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
        void attachDepthBuffer(Texture* texture)
        {
            if (texture->m_width != m_width || texture->m_height != m_height)
            {
                CYAN_ASSERT(0, "Mismatched render target and depth buffer dimension!") 
            }
            glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH24_STENCIL8, GL_TEXTURE_2D, texture->m_id, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            m_depthBuffer = texture;
        }
        void setDrawBuffer(u32 _bufferIdx);
        Texture* getColorBuffer(u32 index);
        bool validate();

        u32 m_width;
        u32 m_height;
        u32 m_numColorBuffers;
        u32 m_numDepthBuffers;
        GLuint m_renderBuffer;
        GLuint m_frameBuffer;
        std::vector<Cyan::Texture*> m_colorBuffers;
        Cyan::Texture* m_depthBuffer;
    };
}