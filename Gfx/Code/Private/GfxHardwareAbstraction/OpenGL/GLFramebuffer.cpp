#include "MathLibrary.h"
#include "GLFramebuffer.h"
#include "GLTexture.h"

namespace Cyan
{
    GLFramebuffer::GLFramebuffer(u32 width, u32 height)
        : GHFramebuffer(width, height)
    {
        glCreateFramebuffers(1, &m_name);
    }

    GLFramebuffer::~GLFramebuffer()
    {

    }

    void GLFramebuffer::bind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_name);
        bBound = true;
    }

    void GLFramebuffer::unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        bBound = false;
    }

    static std::shared_ptr<GLTexture2D> check()
    {

    }

    void GLFramebuffer::bindColorBuffer(GHTexture2D* colorBuffer, u32 bindingUnit)
    {
        assert(bindingUnit < FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS);
        // todo: is there anyway of designing this to avoid this cast?
        auto glTexture = dynamic_cast<GLTexture2D*>(colorBuffer);
        assert(glTexture != nullptr);
        const auto& desc = glTexture->getDesc();
        assert(desc.width == m_width && desc.height == m_height);

        glNamedFramebufferTexture2DEXT(m_name, GL_COLOR_ATTACHMENT0 + bindingUnit, GL_TEXTURE_2D, glTexture->getName(), 0);
        m_colorBuffers[bindingUnit] = colorBuffer;
    }

    void GLFramebuffer::bindColorBuffer(GHTexture2D* colorBuffer, u32 bindingUnit, u32 mipLevel)
    {
        assert(bindingUnit < FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS);
        // todo: is there anyway of designing this to avoid this cast?
        auto glTexture = dynamic_cast<GLTexture2D*>(colorBuffer);
        assert(glTexture != nullptr);
        i32 mipWidth = 0, mipHeight = 0;
        glTexture->getMipSize(mipWidth, mipHeight, mipLevel);
        assert(mipWidth == m_width && mipHeight == m_height);
        glNamedFramebufferTexture2DEXT(m_name, GL_COLOR_ATTACHMENT0 + bindingUnit, GL_TEXTURE_2D, glTexture->getName(), mipLevel);
        m_colorBuffers[bindingUnit] = colorBuffer;
    }

    void GLFramebuffer::unbindColorBuffer(u32 bindingUnit)
    {
        assert(bindingUnit < FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS);

        glNamedFramebufferTexture2DEXT(m_name, GL_COLOR_ATTACHMENT0 + bindingUnit, GL_TEXTURE_2D, 0, 0);
    }

    void GLFramebuffer::clearColorBuffer(u32 bindingUnit, glm::vec4 clearColor)
    {
        ColorBuffer cb = (ColorBuffer)((u32)ColorBuffer::Color0 + bindingUnit);
        i32 drawBuffer = m_drawBufferBindings.findDrawBuffer(cb);
        assert(drawBuffer >= 0);
        glClearNamedFramebufferfv(m_name, GL_COLOR, drawBuffer, &clearColor.x);
    }

    void GLFramebuffer::bindDepthBuffer(GHDepthTexture* depthBuffer)
    {
        auto glTexture = dynamic_cast<GLDepthTexture*>(depthBuffer);
        assert(glTexture != nullptr);
        const auto& desc = glTexture->getDesc();
        assert(desc.width == m_width && desc.height == m_height);

        glNamedFramebufferTexture2DEXT(m_name, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, glTexture->getName(), 0);
        m_depthBuffer = depthBuffer;
    }

    void GLFramebuffer::unbindDepthBuffer()
    {
        glNamedFramebufferTexture2DEXT(m_name, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    }

    void GLFramebuffer::clearDepthBuffer(f32 clearDepth)
    {
        glClearNamedFramebufferfv(m_name, GL_DEPTH, 0, &clearDepth);
    }

    static auto translate = [](const ColorBuffer& colorBuffer) -> GLenum {
        switch (colorBuffer)
        {
        case ColorBuffer::None: return GL_NONE;
        case ColorBuffer::Color0: return GL_COLOR_ATTACHMENT0;
        case ColorBuffer::Color1: return GL_COLOR_ATTACHMENT1;
        case ColorBuffer::Color2: return GL_COLOR_ATTACHMENT2;
        case ColorBuffer::Color3: return GL_COLOR_ATTACHMENT3;
        case ColorBuffer::Color4: return GL_COLOR_ATTACHMENT4;
        case ColorBuffer::Color5: return GL_COLOR_ATTACHMENT5;
        case ColorBuffer::Color6: return GL_COLOR_ATTACHMENT6;
        case ColorBuffer::Color7: return GL_COLOR_ATTACHMENT7;
        default: assert(0); return GL_NONE;
        }
    };

    void GLFramebuffer::setDrawBuffers(const DrawBufferBindings& drawBufferBindings)
    {
        m_drawBufferBindings = drawBufferBindings;
        GLenum drawBuffers[DrawBufferBindings::kNumDrawBuffers];
        for (i32 i = 0; i < DrawBufferBindings::kNumDrawBuffers; ++i)
        {
            drawBuffers[i] = translate(drawBufferBindings[i]);
        }
        glNamedFramebufferDrawBuffers(m_name, DrawBufferBindings::kNumDrawBuffers, drawBuffers);
    }

    void GLFramebuffer::unsetDrawBuffers()
    {
        DrawBufferBindings defaultBindings{ };
        setDrawBuffers(defaultBindings);
    }
}

