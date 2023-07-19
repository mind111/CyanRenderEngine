#pragma once

#include "GLObject.h"
#include "GHFramebuffer.h"

namespace Cyan
{
    class GLFramebuffer : public GLObject, public GHFramebuffer
    {
    public:
        GLFramebuffer(u32 width, u32 height);
        virtual ~GLFramebuffer();

        virtual void bind() override;
        virtual void unbind() override;
        virtual void bindColorBuffer(GHTexture2D* colorBuffer, u32 bindingUnit) override;
        virtual void bindColorBuffer(GHTexture2D* colorBuffer, u32 bindingUnit, u32 mipLevel) override;
        virtual void unbindColorBuffer(u32 bindingUnit) override;
        virtual void bindDepthBuffer(GHDepthTexture* depthBuffer) override;
        virtual void unbindDepthBuffer() override;
        virtual void setDrawBuffers(const DrawBufferBindings& drawBufferBindings) override;
        virtual void unsetDrawBuffers() override;

        bool isBound() { return bBound; }
    private:
        bool bBound = false;
    };
}
