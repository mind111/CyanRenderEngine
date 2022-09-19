#include "RenderTarget.h"

namespace Cyan
{
    RenderTarget* RenderTarget::defaultRenderTarget = nullptr;

    RenderTarget* RenderTarget::getDefaultRenderTarget(u32 width, u32 height)
    { 
        if (!defaultRenderTarget)
        {
            defaultRenderTarget = new RenderTarget();
            defaultRenderTarget->width = width;
            defaultRenderTarget->height = height;
            defaultRenderTarget->fbo = 0;
        }
        else
        {
            if (width != defaultRenderTarget->width || height != defaultRenderTarget->height)
            {
                delete defaultRenderTarget;

                defaultRenderTarget = new RenderTarget();
                defaultRenderTarget->width = width;
                defaultRenderTarget->height = height;
                defaultRenderTarget->fbo = 0;
            }
        }
        return defaultRenderTarget;
    }
    
    ITextureRenderable* RenderTarget::getColorBuffer(u32 index)
    {
        return colorBuffers[index];
    }

    void RenderTarget::setColorBuffer(Texture2DRenderable* texture, u32 index, u32 mip)
    {
        if (index > 7)
        {
            cyanError("Drawbuffer index out of bound!");
            return;
        }
        glNamedFramebufferTexture2DEXT(fbo, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, texture->getGpuResource(), mip);
        colorBuffers[index] = texture;
    }

    void RenderTarget::setColorBuffer(TextureCubeRenderable* texture, u32 index, u32 mip)
    {
        if (index > 7)
        {
            cyanError("Drawbuffer index out of bound!");
        }

        const u32 numFaces = 6u;
        if (index + numFaces - 1u > 7u)
        {
            cyanError("Too many drawbuffers bind to a framebuffer object!");
        }
        else
        {
            for (i32 f = 0; f < numFaces; ++f)
            {
                glNamedFramebufferTexture2DEXT(fbo, GL_COLOR_ATTACHMENT0 + (index + f), GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, texture->getGpuResource(), mip);
                colorBuffers[index + f] = static_cast<ITextureRenderable*>(texture);
            }
        }
    }

    void RenderTarget::setDepthBuffer(DepthTexture2D* texture)
    {
        if (texture->width != width || texture->height != height)
        {
            CYAN_ASSERT(0, "Mismatched render target and depth buffer dimension!") 
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture->getGpuResource(), 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        depthBuffer = texture;
    }

    void RenderTarget::clear(const std::initializer_list<RenderTargetDrawBuffer>& buffers, f32 clearDepth)
    {
        // clear specified color buffer
        for (i32 i = 0; i < buffers.size(); ++i)
        {
            RenderTargetDrawBuffer& drawBuffer = const_cast<RenderTargetDrawBuffer&>(*(buffers.begin() + i));
            glClearNamedFramebufferfv(fbo, GL_COLOR, drawBuffer.binding, &drawBuffer.clearColor.x);
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