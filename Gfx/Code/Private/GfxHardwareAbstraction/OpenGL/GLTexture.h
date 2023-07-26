#pragma once

#include "Core.h"
#include "GHTexture.h"
#include "GLObject.h"

namespace Cyan
{
#define INVALID_TEXTURE_UNIT -1
    class GLSampler2D
    {

    };

    class GLTexture : public GLObject
    {
    public:
        virtual ~GLTexture();
        void bind();
        void unbind();
        bool isBound();
        i32 getBoundTextureUnit() { return m_boundTextureUnit; }
    protected:
        GLTexture();

        static void initTexture2D(GLuint glTextureObject, const GHTexture2D::Desc& desc, const GHSampler2D& defaultSampler);

        GLuint m_boundTextureUnit = INVALID_TEXTURE_UNIT;
    private:
    };

    class GLTexture2D : public GLTexture, public GHTexture2D
    {
    public:
        GLTexture2D(const GHTexture2D::Desc& desc);
        virtual ~GLTexture2D();

        virtual void init() override;
        virtual void bind() override;
        virtual void unbind() override;
        virtual void getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel) override;
    };

    class GLDepthTexture : public GLTexture, public GHDepthTexture
    {
    public:
        GLDepthTexture(const GHDepthTexture::Desc& desc);
        virtual ~GLDepthTexture();

        virtual void init() override;
        virtual void bind() override;
        virtual void unbind() override;
        virtual void getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel) override;
    };
}
