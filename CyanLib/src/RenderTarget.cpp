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
    
    ITexture* RenderTarget::getColorBuffer(u32 index)
    {
        return colorBuffers[index];
    }

    void RenderTarget::setDrawBuffers(const std::initializer_list<i32>& drawBuffers) {
        GLenum* buffers = static_cast<GLenum*>(_alloca(drawBuffers.size() * sizeof(GLenum)));
        i32 numBuffers = drawBuffers.size();
        for (i32 i = 0; i < numBuffers; ++i)
        {
            i32 drawBuffer = *(drawBuffers.begin() + i);
            if (drawBuffer > -1)
            {
                buffers[i] = GL_COLOR_ATTACHMENT0 + drawBuffer;
            }
            else
            {
                buffers[i] = GL_NONE;
            }

        }
        if (numBuffers > 0) {
            glNamedFramebufferDrawBuffers(fbo, numBuffers, buffers);
        }
    }

    void RenderTarget::setColorBuffer(Texture2D* texture, u32 index, u32 mip)
    {
        if (index > 7)
        {
            cyanError("Drawbuffer index out of bound!");
            return;
        }
        glNamedFramebufferTexture2DEXT(fbo, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, texture->getGpuObject(), mip);
        colorBuffers[index] = texture;
    }

    void RenderTarget::setColorBuffer(TextureCube* texture, u32 index, u32 mip)
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
                glNamedFramebufferTexture2DEXT(fbo, GL_COLOR_ATTACHMENT0 + (index + f), GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, texture->getGpuObject(), mip);
                colorBuffers[index + f] = static_cast<ITexture*>(texture);
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
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture->getGpuObject(), 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        depthBuffer = texture;
    }

    void RenderTarget::clearDrawBuffer(i32 drawBufferIndex, glm::vec4 clearColor, bool clearDepth, f32 clearDepthValue) {
        glClearNamedFramebufferfv(fbo, GL_COLOR, drawBufferIndex, &clearColor.x);
        if (clearDepth) {
            clearDepthBuffer(clearDepthValue);
        }
    }

    void RenderTarget::clearDepthBuffer(f32 clearDepthValue) {
        // clear depth buffer
        glClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &clearDepthValue);
    }

    void RenderTarget::clear(const std::initializer_list<RenderTargetDrawBuffer>& buffers, f32 clearDepthBuffer)
    {
        // clear specified albedo buffer
        for (i32 i = 0; i < buffers.size(); ++i)
        {
            RenderTargetDrawBuffer& drawBuffer = const_cast<RenderTargetDrawBuffer&>(*(buffers.begin() + i));
            /** note - @min: the drawBuffer.binding here is used to index into currently bound draw buffers rather
            * than index into albedo attachments. for example, passing an index of 0 will refer to first bound draw buffer which
            * can be an arbitrary albedo attachment as long as it's bound as the first draw buffer
            */
            glClearNamedFramebufferfv(fbo, GL_COLOR, drawBuffer.binding, &drawBuffer.clearColor.x);
        }
        // clear depth buffer
        glClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &clearDepthBuffer);
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