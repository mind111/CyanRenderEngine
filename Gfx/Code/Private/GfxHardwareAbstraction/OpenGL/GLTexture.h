#pragma once

#include "Core.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "GLObject.h"

namespace Cyan
{
#define INVALID_TEXTURE_UNIT -1
#define INVALID_IMAGE_UNIT -1

    class GLSampler2D
    {
    };

    class GLTexture : public GLObject
    {
    public:
        virtual ~GLTexture();
        void bindAsTexture();
        void unbindAsTexture();
        // todo: support binding a layer at some point when it becomes necessary
        void bindAsImage(u32 mipLevel, const PixelFormat& pf);
        void unbindAsImage();
        bool isBoundAsTexture();
        bool isBoundAsImage();
        i32 getBoundTextureUnit() { return m_boundTextureUnit; }
        i32 getBoundImageUnit() { return m_boundImageUnit; }
    protected:
        GLTexture();

        static void initTexture2D(GLuint glTextureObject, const GHTexture2D::Desc& desc, const GHSampler2D& defaultSampler);
        static void initTexture3D(GLuint glTextureObject, const GHTexture3D::Desc& desc, const GHSampler3D& defaultSampler);
        static void initTextureCube(GLuint glTextureObject, const GHTextureCube::Desc& desc, const GHSamplerCube& defaultSampler);

        GLuint m_boundTextureUnit = INVALID_TEXTURE_UNIT;
        GLuint m_boundImageUnit = INVALID_IMAGE_UNIT;
    private:
    };

    class GLTexture2D : public GLTexture, public GHTexture2D
    {
    public:
        GLTexture2D(const GHTexture2D::Desc& desc, const GHSampler2D& sampler2D = GHSampler2D());
        virtual ~GLTexture2D(); 

        virtual void init() override;
        virtual void bindAsTexture() override;
        virtual void unbindAsTexture() override;
        virtual void bindAsRWTexture(u32 mipLevel) override;
        virtual void unbindAsRWTexture() override;
        virtual void* getGHO() final;
        virtual void getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel) override;
    };

    class GLDepthTexture : public GLTexture, public GHDepthTexture
    {
    public:
        GLDepthTexture(const GHDepthTexture::Desc& desc);
        virtual ~GLDepthTexture();

        virtual void init() override;
        virtual void bindAsTexture() override;
        virtual void unbindAsTexture() override;
        virtual void bindAsRWTexture(u32 mipLevel) override;
        virtual void unbindAsRWTexture() override;
        virtual void getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel) override;
    };

    class GLTextureCube : public GLTexture, public GHTextureCube
    {
    public:
        GLTextureCube(const GHTextureCube::Desc& desc, const GHSamplerCube& samplerCube);
        virtual ~GLTextureCube();

        virtual void init() override;
        virtual void bindAsTexture() override;
        virtual void unbindAsTexture() override;
        virtual void bindAsRWTexture(u32 mipLevel) override;
        virtual void unbindAsRWTexture() override;
        virtual void getMipSize(i32& outSize, i32 mipLevel) override;
    };

    class GLTexture3D : public GLTexture, public GHTexture3D
    {
    public:
        GLTexture3D(const GHTexture3D::Desc& desc, const GHSampler3D& sampler3D);
        virtual ~GLTexture3D();

        virtual void init() override;
        virtual void bindAsTexture() override;
        virtual void unbindAsTexture() override;
        virtual void bindAsRWTexture(u32 mipLevel) override;
        virtual void unbindAsRWTexture() override;
        virtual void getMipSize(i32& outWidth, i32& outHeight, i32& outDepth, i32 mipLevel) override;
    };
}
