#pragma once

#include <string>

#include "glew.h"

#include "Common.h"
#include "CyanCore.h"
#include "Asset.h"

namespace Cyan
{
    struct ITextureRenderable : public Asset, public GpuResource
    {
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("ITextureRenderable");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("ITextureRenderable"); 
        }

        struct Spec
        {
            enum class Type
            {
                kTex2D,
                kDepthTex,
                kTex3D,
                kTexCube,
                kCount
            };

            enum class PixelFormat
            {
                R16F = 0,
                R32F,
                Lum32F,
                R32G32F,
                R8G8B8,
                D24S8,
                R16G16B16,
                R32G32B32,
                R8G8B8A8,
                R16G16B16A16,
                R32G32B32A32,
                kInvalid
            };

            u32 width = 1;
            u32 height = 1;
            u32 depth = 1;
            u32 numMips = 1;
            Type type = Type::kCount;
            PixelFormat pixelFormat = PixelFormat::kInvalid;
            u8* pixelData = nullptr;
        };

        struct Parameter
        {
            enum class Filtering
            {
                LINEAR = 0,
                NEAREST,
                MIPMAP_LINEAR,
            };

            enum class Wrap
            {
                CLAMP_TO_EDGE = 0,
                NONE
            };

            Filtering minificationFilter = Filtering::LINEAR;
            Filtering magnificationFilter = Filtering::LINEAR;
            Wrap wrap_s = Wrap::CLAMP_TO_EDGE;
            Wrap wrap_r = Wrap::CLAMP_TO_EDGE;
            Wrap wrap_t = Wrap::CLAMP_TO_EDGE;
        };

        virtual Spec getTextureSpec() = 0;

        ITextureRenderable(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : GpuResource(),
            name(inName),
            pixelFormat(inSpec.pixelFormat),
            parameter(inParams),
            numMips(inSpec.numMips),
            pixelData(inSpec.pixelData)
        { }

        virtual ~ITextureRenderable() 
        { 
            glDeleteTextures(1, &glResource);
            if (pixelData)
            {
                delete[] pixelData;
            }
        }

        struct GLPixelFormat
        {
            GLint internalFormat;
            GLenum format;
            GLenum type;
        };

        static GLPixelFormat translatePixelFormat(const Spec::PixelFormat& inPixelFormat)
        {
            GLPixelFormat glPixelFormat = { };
            switch (inPixelFormat)
            {
            case Spec::PixelFormat::Lum32F:
                glPixelFormat.internalFormat = GL_LUMINANCE8;
                glPixelFormat.format = GL_LUMINANCE;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R16F:
                glPixelFormat.internalFormat = GL_R16F;
                glPixelFormat.format = GL_R;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R32F:
                glPixelFormat.internalFormat = GL_R32F;
                glPixelFormat.format = GL_R;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::D24S8:
                glPixelFormat.internalFormat = GL_DEPTH24_STENCIL8;
                glPixelFormat.format = GL_DEPTH_STENCIL;
                glPixelFormat.type = GL_UNSIGNED_INT_24_8;
                break;
            case Spec::PixelFormat::R8G8B8:
                glPixelFormat.internalFormat = GL_RGB8;
                glPixelFormat.format = GL_RGB;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
            case Spec::PixelFormat::R16G16B16:
                glPixelFormat.internalFormat = GL_RGB16F;
                glPixelFormat.format = GL_RGB;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R32G32B32:
                glPixelFormat.internalFormat = GL_RGB32F;
                glPixelFormat.format = GL_RGB;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R8G8B8A8:
                glPixelFormat.internalFormat = GL_RGBA8;
                glPixelFormat.format = GL_RGBA;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
                break;
            case Spec::PixelFormat::R32G32B32A32:
                glPixelFormat.internalFormat = GL_RGBA32F;
                glPixelFormat.format = GL_RGBA;
                glPixelFormat.type = GL_FLOAT;
                break;
            default:
                break;
            }
            return glPixelFormat;
        }

        static void initializeTextureParameters(GLuint textureObject, const Parameter& parameter)
        {
            auto translateFiltering = [](const Parameter::Filtering filtering) {
                switch (filtering)
                {
                case Parameter::Filtering::LINEAR:
                    return GL_LINEAR;
                case Parameter::Filtering::MIPMAP_LINEAR:
                    return GL_LINEAR_MIPMAP_LINEAR;
                case Parameter::Filtering::NEAREST:
                    return GL_NEAREST;
                default:
                    return GLU_INVALID_VALUE;
                }
            };

            auto translateWrap = [](const Parameter::Wrap wrap) {
                switch (wrap)
                {
                case Parameter::Wrap::CLAMP_TO_EDGE:
                    return GL_CLAMP_TO_EDGE;
                case Parameter::Wrap::NONE:
                default:
                    return GL_REPEAT;
                }
            };

            glTextureParameteri(textureObject, GL_TEXTURE_MIN_FILTER, translateFiltering(parameter.minificationFilter));
            glTextureParameteri(textureObject, GL_TEXTURE_MAG_FILTER, translateFiltering(parameter.magnificationFilter));
            glTextureParameteri(textureObject, GL_TEXTURE_WRAP_S, translateWrap(parameter.wrap_s));
            glTextureParameteri(textureObject, GL_TEXTURE_WRAP_T, translateWrap(parameter.wrap_t));
            glTextureParameteri(textureObject, GL_TEXTURE_WRAP_R, translateWrap(parameter.wrap_r));
        }

        const char* name = nullptr;
        Spec::PixelFormat pixelFormat = Spec::PixelFormat::kInvalid;
        Parameter parameter = { };
        u32 numMips = 0;
        u8* pixelData = nullptr;
    };

    struct Texture2DRenderable : public ITextureRenderable
    {
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("Texture2DRenderable");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("Texture2DRenderable"); 
        }

        virtual Spec getTextureSpec() override
        {
            return Spec {
                width, /* width */
                height, /* height */
                1, /* depth */
                numMips, /* numMips */
                Spec::Type::kTex2D, /* texture type */
                pixelFormat, /* pixel format */
                pixelData /* pixel data */
            };
        }

        Texture2DRenderable(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : ITextureRenderable(inName, inSpec, inParams),
            width(inSpec.width),
            height(inSpec.height)
        {
            glCreateTextures(GL_TEXTURE_2D, 1, &glResource);
            glBindTexture(GL_TEXTURE_2D, getGpuResource());
            auto glPixelFormat = translatePixelFormat(pixelFormat);
            glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, pixelData);
            glBindTexture(GL_TEXTURE_2D, 0);

            initializeTextureParameters(getGpuResource(), parameter);

            if (numMips > 1u)
            {
                glGenerateTextureMipmap(getGpuResource());
            }
        }

        ~Texture2DRenderable()
        {
            ITextureRenderable::~ITextureRenderable();
        }

        u32 width;
        u32 height;
    };

    struct DepthTexture : public Texture2DRenderable
    {
        /* Asset interface */
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("DepthTexture");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("DepthTexture"); 
        }

        /* ITextureRenderable interface */
        virtual Spec getTextureSpec() override
        {
            return Spec {
                width, /* width */
                height, /* height */
                1, /* depth */
                numMips, /* numMips */
                Spec::Type::kDepthTex, /* texture type */
                pixelFormat, /* pixel format */
                pixelData /* pixel data */
            };
        }

        DepthTexture(const char* name, u32 width, u32 height)
            : Texture2DRenderable(
                name,
                Spec {
                    width, /* width */
                    height, /* height */
                    1, /* depth */
                    1, /* numMips */
                    Spec::Type::kTex2D, /* texture type */
                    Spec::PixelFormat::D24S8, /* pixel format */
                    nullptr /* pixel data */
                }
            )
        { }

        ~DepthTexture()
        {
            ITextureRenderable::~ITextureRenderable();
        }
    };

    struct Texture3DRenderable : public ITextureRenderable
    {
        /* Asset interface */
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("Texture3DRenderable");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("Texture3DRenderable"); 
        }

        /* ITextureRenderable interface */
        virtual Spec getTextureSpec() override
        {
            return Spec {
                width, /* width */
                height, /* height */
                depth, /* depth */
                numMips, /* numMips */
                Spec::Type::kTex3D, /* texture type */
                pixelFormat, /* pixel format */
                pixelData /* pixel data */
            };
        }

        Texture3DRenderable(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : ITextureRenderable(inName, inSpec, inParams),
            width(inSpec.width),
            height(inSpec.height),
            depth(inSpec.depth)
        {
            glCreateTextures(GL_TEXTURE_3D, 1, &glResource);
            glBindTexture(GL_TEXTURE_3D, getGpuResource());
            auto glPixelFormat = translatePixelFormat(pixelFormat);
            glTexImage3D(GL_TEXTURE_3D, 0, glPixelFormat.internalFormat, width, height, depth, 0, glPixelFormat.format, glPixelFormat.type, pixelData);
            glBindTexture(GL_TEXTURE_3D, 0);

            initializeTextureParameters(getGpuResource(), parameter);

            if (numMips > 1u)
            {
                glGenerateTextureMipmap(getGpuResource());
            }
        }

        ~Texture3DRenderable()
        {
            ITextureRenderable::~ITextureRenderable();
        }

        u32 width;
        u32 height;
        u32 depth;
    };

    struct TextureCubeRenderable : public ITextureRenderable
    {
        /* Asset interface */
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("TextureCubeRenderable");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("TextureCubeRenderable"); 
        }

        /* ITextureRenderable interface */
        virtual Spec getTextureSpec() override
        {
            return Spec {
                resolution, /* width */
                resolution, /* height */
                1, /* depth */
                numMips, /* numMips */
                Spec::Type::kTexCube, /* texture type */
                pixelFormat, /* pixel format */
                pixelData /* pixel data */
            };
        }

        TextureCubeRenderable(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : ITextureRenderable(inName, inSpec, inParams),
            resolution(inSpec.width)
        {
            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &glResource);
            glBindTexture(GL_TEXTURE_CUBE_MAP, getGpuResource());
            auto glPixelFormat = translatePixelFormat(pixelFormat);
            for (i32 f = 0; f < 6; ++f)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, glPixelFormat.internalFormat, resolution, resolution, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
            }
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            initializeTextureParameters(getGpuResource(), parameter);

            if (numMips > 1u)
            {
                glGenerateTextureMipmap(getGpuResource());
            }
        }

        ~TextureCubeRenderable()
        {
            ITextureRenderable::~ITextureRenderable();
        }

        u32 resolution;
    };
}