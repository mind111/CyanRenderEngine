#include "RenderTarget.h"

namespace Cyan
{
    void RenderTarget::attachColorBuffer(Texture* _texture, u32 mip)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        switch (_texture->m_type)
        {
            case Texture::Type::TEX_2D:
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_numColorBuffers, GL_TEXTURE_2D, _texture->m_id, mip);
                m_numColorBuffers += 1;
                break;
            case Texture::Type::TEX_CUBEMAP:
                for (u32 f = 0; f < 6u; f++)
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_numColorBuffers, GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, _texture->m_id, mip);
                    m_numColorBuffers += 1;
                }
                break;
            default:
                break;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_colorBuffers.push_back(_texture);
    }

    // TODO: substitude attachColorBuffer() with this
    void RenderTarget::attachTexture(Texture* texture, u32 attachmentIndex, u32 faceIndex)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        switch (texture->m_type)
        {
            case Texture::Type::TEX_2D:
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachmentIndex, GL_TEXTURE_2D, texture->m_id, 0);
                m_textures[attachmentIndex] = texture;
                break;
            case Texture::Type::TEX_CUBEMAP:
                CYAN_ASSERT(faceIndex != -1, "Cannot attahc a cubemap as color attachment without specifiying which cubemap face to attach")
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachmentIndex, GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIndex, texture->m_id, 0);
                m_textures[attachmentIndex] = texture;
                break;
            default:
                break;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // TODO: implement this
    void RenderTarget::setColorBuffer(Texture* texture, u32 index)
    {
        CYAN_ASSERT(m_colorBuffers.size() == index, "index %u is already bound", index)
        // m_colorBuffers[index] must be empty
    }

    Texture* RenderTarget::getColorBuffer(u32 index)
    {
        return m_colorBuffers[index];
    }

    void RenderTarget::setDrawBuffer(u32 _bufferIdx)
    {
        glNamedFramebufferDrawBuffer(m_frameBuffer, GL_COLOR_ATTACHMENT0 + _bufferIdx);
    }

    void RenderTarget::bindDrawBuffers()
    {
        CYAN_ASSERT(m_numDrawBuffers > 0, "No draw buffers are set for render target \n");
        GLenum* drawBuffers = (GLenum*)_alloca(sizeof(GLenum) * m_numDrawBuffers);
        for (u32 i = 0; i < m_numDrawBuffers; ++i)
        {
            if (m_drawBuffers[i] == -1)
                drawBuffers[i] = GL_NONE;
            else
            {
                drawBuffers[i] = GL_COLOR_ATTACHMENT0 + m_drawBuffers[i];
            }
        }
        glNamedFramebufferDrawBuffers(m_frameBuffer, m_numDrawBuffers, drawBuffers);
    }

    void RenderTarget::setDrawBuffers(i32* buffers, u32 numBuffers)
    {
        memset(m_drawBuffers, 0x0, sizeof(m_drawBuffers));
        memcpy(m_drawBuffers, buffers, numBuffers * sizeof(i32));
        m_numDrawBuffers = numBuffers;
    }

    bool RenderTarget::validate()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            printf("ERROR::FRAMEBUFFER:: Framebuffer %d is not complete", m_frameBuffer);
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void RenderTarget::setDepthBuffer(Texture* texture)
    {
        if (texture->m_width != m_width || texture->m_height != m_height)
        {
            CYAN_ASSERT(0, "Mismatched render target and depth buffer dimension!") 
        }
        glBindFramebuffer(GL_FRAMEBUFFER, m_frameBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture->m_id, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_depthBuffer = texture;
    }
}