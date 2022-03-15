#include "RenderTarget.h"

namespace Cyan
{
    Texture* RenderTarget::getColorBuffer(u32 index)
    {
        return colorBuffers[index];
    }

    void RenderTarget::setColorBuffer(Texture* texture, u32 index, u32 mip)
    {
        if (index > 7)
        {
            cyanError("Drawbuffer index out of bound!");
        }
        switch (texture->m_type)
        {
        case Texture::TEX_2D:
            glNamedFramebufferTexture2DEXT(fbo, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, texture->m_id, mip);
            colorBuffers[index] = texture;
            break;
        case Texture::TEX_CUBEMAP:
        {
            const u32 numFaces = 6u;
            if (index + numFaces - 1u > 7u)
            {
                cyanError("Too many drawbuffers bind to a framebuffer object!");
            }
            else
            {
                for (i32 f = 0; f < numFaces; ++f)
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (index + f), GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, texture->m_id, mip);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    void RenderTarget::setDepthBuffer(Texture* texture)
    {
        if (texture->m_width != width || texture->m_height != height)
        {
            CYAN_ASSERT(0, "Mismatched render target and depth buffer dimension!") 
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture->m_id, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        depthBuffer = texture;
    }

    void RenderTarget::clear(const std::initializer_list<i32>& buffers, glm::vec4 clearColor, f32 clearDepth)
    {
        // clear specified color buffer
        for (i32 i = 0; i < buffers.size(); ++i)
        {
            glClearNamedFramebufferfv(fbo, GL_COLOR, *(buffers.begin() + i), &clearColor.x);
        }
        // clear depth buffer
        glClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &clearDepth);
    }

    bool RenderTarget::validate()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            printf("ERROR::FRAMEBUFFER:: Framebuffer %d is not complete", fbo);
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

}