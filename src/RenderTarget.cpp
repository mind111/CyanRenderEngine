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
}