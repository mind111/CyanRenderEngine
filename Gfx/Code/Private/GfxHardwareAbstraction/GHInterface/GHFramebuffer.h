#pragma once

#include "Core.h"
#include "GHTexture.h"
#include "MathLibrary.h"

namespace Cyan
{
#define FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS 8

    enum class ColorBuffer : i32
    {
        None = -1,
        Color0 = 0,
        Color1,
        Color2,
        Color3,
        Color4,
        Color5,
        Color6,
        Color7,
    };

    struct DrawBufferBindings
    {
        const ColorBuffer& operator[](i32 slot) const
        { 
            assert(slot < kNumDrawBuffers);
            return slots[slot];
        }

        void bind(const ColorBuffer& cb, u32 slot)
        {
            slots[slot] = cb;
        }

        i32 findDrawBuffer(const ColorBuffer& cb)
        {
            for (u32 i = 0; i < kNumDrawBuffers; ++i)
            {
                if (slots[i] == cb)
                {
                    return (i32)i;
                }
            }
            return -1;
        }

        static constexpr u32 kNumDrawBuffers = FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS;
        ColorBuffer slots[kNumDrawBuffers] = { 
            ColorBuffer::None, 
            ColorBuffer::None, 
            ColorBuffer::None, 
            ColorBuffer::None, 
            ColorBuffer::None,
            ColorBuffer::None, 
            ColorBuffer::None, 
            ColorBuffer::None
        };
    };

    class GHFramebuffer
    {
    public:
        virtual ~GHFramebuffer();
        virtual void bind() = 0;
        virtual void unbind() = 0;
        virtual void bindColorBuffer(GHTexture2D* colorBuffer, u32 bindingUnit) = 0;
        virtual void bindColorBuffer(GHTexture2D* colorBuffer, u32 bindingUnit, u32 mipLevel) = 0;
        virtual void unbindColorBuffer(u32 bindingUnit) = 0;
        virtual void clearColorBuffer(u32 bindingUnit, glm::vec4 clearColor) = 0;
        virtual void bindDepthBuffer(GHDepthTexture* depthBuffer) = 0;
        virtual void unbindDepthBuffer() = 0;
        virtual void clearDepthBuffer(f32 clearDepth) = 0;
        virtual void setDrawBuffers(const DrawBufferBindings& drawBufferBindings) = 0;
        virtual void unsetDrawBuffers() = 0;
    protected:
        GHFramebuffer(u32 width, u32 height);

        std::array<GHTexture*, FRAMEBUFFER_MAX_NUM_COLOR_BUFFERS> m_colorBuffers;
        GHDepthTexture* m_depthBuffer = nullptr;
        u32 m_width = 0, m_height = 0;
    };
}
