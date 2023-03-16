#include "Framebuffer.h"

namespace Cyan
{
    Framebuffer* Framebuffer::create(u32 inWidth, u32 inHeight)
    {
        return new Framebuffer(inWidth, inHeight);
    }

    void Framebuffer::setColorBuffer(GfxTexture2D* texture, u32 index, u32 mip)
    {
        assert(index < kNumColorbufferBindings);
        // make sure we are not leaking states
        assert(colorBuffers[index] == nullptr);

        glNamedFramebufferTexture2DEXT(getGpuResource(), GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, texture->getGpuResource(), mip);
        colorBuffers[index] = texture;
    }

    // layer is basically face
    void Framebuffer::setColorBuffer(GfxTextureCube* texture, u32 index, u32 layer, u32 mip)
    {
        glNamedFramebufferTexture2DEXT(getGpuResource(), GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, texture->getGpuResource(), mip);
        colorBuffers[index] = texture;
    }

#if 0
    void Framebuffer::setColorBuffer(TextureCube* texture, u32 index, u32 mip)
    {
        const u32 numFaces = 6u;
        if ((index + numFaces - 1u) < kNumColorbufferBindings)
        {
            for (i32 f = 0; f < numFaces; ++f)
            {
                glNamedFramebufferTexture2DEXT(getGpuResource(), GL_COLOR_ATTACHMENT0 + (index + f), GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, texture->getGpuResource(), mip);
                colorBuffers[index + f] = texture;
            }
        }
        else
        {
            assert(0);
        }
    }
#endif
    
    void Framebuffer::setDepthBuffer(GfxDepthTexture2D* depthTexture)
    {
        assert(depthTexture->width == width && depthTexture->height == height);
        assert(depthBuffer == nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, getGpuResource());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTexture->getGpuResource(), 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        depthBuffer = depthTexture;
    }

    void Framebuffer::setDrawBuffers(i32* colorBufferIndices, u32 count)
    {
        GLenum* drawBuffers = static_cast<GLenum*>(_alloca(count * sizeof(GLenum)));
        for (i32 i = 0; i < count; ++i)
        {
            i32 colorBufferIndex = colorBufferIndices[i];
            if (colorBufferIndex > -1)
            {
                drawBuffers[i] = GL_COLOR_ATTACHMENT0 + colorBufferIndex;
            }
            else
            {
                drawBuffers[i] = GL_NONE;
            }
        }
        if (count > 0) 
        {
            glNamedFramebufferDrawBuffers(getGpuResource(), count, drawBuffers);
        }
    }

    void Framebuffer::setDrawBuffers(const std::initializer_list<i32>& colorBufferIndices) 
    {
        i32 numDrawBuffers = colorBufferIndices.size();
        GLenum* drawBuffers = static_cast<GLenum*>(_alloca(numDrawBuffers * sizeof(GLenum)));
        for (i32 i = 0; i < numDrawBuffers; ++i)
        {
            i32 colorBufferIndex = *(colorBufferIndices.begin() + i);
            if (colorBufferIndex > -1)
            {
                drawBuffers[i] = GL_COLOR_ATTACHMENT0 + colorBufferIndex;
            }
            else
            {
                drawBuffers[i] = GL_NONE;
            }
        }
        if (numDrawBuffers > 0) 
        {
            glNamedFramebufferDrawBuffers(getGpuResource(), numDrawBuffers, drawBuffers);
        }
    }

    void Framebuffer::clearDrawBuffer(i32 drawBufferIndex, glm::vec4 clearColor) 
    {
        glClearNamedFramebufferfv(getGpuResource(), GL_COLOR, drawBufferIndex, &clearColor.x);
    }

    void Framebuffer::clearDepthBuffer(f32 clearDepthValue) 
    {
        glClearNamedFramebufferfv(getGpuResource(), GL_DEPTH, 0, &clearDepthValue);
    }

    void Framebuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, getGpuResource());
    }

    void Framebuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool Framebuffer::validate()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, getGpuResource());
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

    void Framebuffer::reset()
    {
        // unbind all color attachments
        for (i32 i = 0; i < kNumColorbufferBindings; ++i)
        {
            if (colorBuffers[i] != nullptr)
            {
                colorBuffers[i] = nullptr;
                glNamedFramebufferTexture2DEXT(getGpuResource(), GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
            }
        }
        // unbind depth attachment
        if (depthBuffer)
        {
            depthBuffer = nullptr;
            glNamedFramebufferTexture2DEXT(getGpuResource(), GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        }
    }
}