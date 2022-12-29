#pragma once

#include <string>

#include "glew.h"

#include "Common.h"
#include "CyanCore.h"
#include "Asset.h"

/**
* convenience macros
*/
#define TEX_2D Cyan::ITextureRenderable::Spec::Type::kTex2D
#define TEX_DEPTH Cyan::ITextureRenderable::Spec::Type::kDepthTex
#define TEX_3D Cyan::ITextureRenderable::Spec::Type::kTex3D
#define TEX_CUBE Cyan::ITextureRenderable::Spec::Type::kTexCube

// WM stands for "wrap mode"
#define WM_CLAMP Cyan::ITextureRenderable::Parameter::WrapMode::CLAMP_TO_EDGE
#define WM_WRAP Cyan::ITextureRenderable::Parameter::WrapMode::WRAP
// FM stands for "filter mode"
#define FM_POINT Cyan::ITextureRenderable::Parameter::Filtering::NEAREST
#define FM_BILINEAR Cyan::ITextureRenderable::Parameter::Filtering::LINEAR
#define FM_TRILINEAR Cyan::ITextureRenderable::Parameter::Filtering::LINEAR_MIPMAP_LINEAR
#define FM_MIPMAP_POINT Cyan::ITextureRenderable::Parameter::Filtering::NEAREST_MIPMAP_NEAREST
// PF stands for "pixel format" 
#define PF_RGB16F Cyan::ITextureRenderable::Spec::PixelFormat::RGB16F
#define PF_RGBA16F Cyan::ITextureRenderable::Spec::PixelFormat::RGBA16F
#define PF_RGB32F Cyan::ITextureRenderable::Spec::PixelFormat::RGB32F
#define PF_RGBA32F Cyan::ITextureRenderable::Spec::PixelFormat::RGBA32F

namespace Cyan
{
    struct ITextureRenderable : public Asset, public GpuObject
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
                RG16F,
                RG32F,
                RGB8,
                D24S8,
                RGB16F,
                RGB32F,
                RGBA8,
                RGBA16F,
                RGBA32F,
                kInvalid
            };

            bool operator==(const Spec& rhs) const
            {
                return (type == rhs.type) && (width == rhs.width) && (height == rhs.height) && (pixelFormat == rhs.pixelFormat);
            }

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
                LINEAR_MIPMAP_LINEAR,
                NEAREST_MIPMAP_NEAREST
            };

            enum class WrapMode
            {
                CLAMP_TO_EDGE = 0,
                WRAP,
                NONE
            };

            Filtering minificationFilter = Filtering::LINEAR;
            Filtering magnificationFilter = Filtering::LINEAR;
            WrapMode wrap_s = WrapMode::CLAMP_TO_EDGE;
            WrapMode wrap_r = WrapMode::CLAMP_TO_EDGE;
            WrapMode wrap_t = WrapMode::CLAMP_TO_EDGE;
        };

        virtual Spec getTextureSpec() = 0;

        ITextureRenderable(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : GpuObject(),
            pixelFormat(inSpec.pixelFormat),
            parameter(inParams),
            numMips(inSpec.numMips),
            pixelData(inSpec.pixelData)
        { 
            u32 nameLen = strlen(inName) + 1;
            if (nameLen <= 1)
            {
                cyanError("Invalid texture name length.");
            }
            name = new char[nameLen];
            strcpy_s(name, sizeof(char) * nameLen, inName);
        }

        virtual ~ITextureRenderable() 
        { 
            glDeleteTextures(1, &glObject);
            if (name)
            {
                delete[] name;
                name = nullptr;
            }
            if (pixelData)
            {
                delete[] pixelData;
                pixelData = nullptr;
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
            case Spec::PixelFormat::RG16F:
                glPixelFormat.internalFormat = GL_RG16F;
                glPixelFormat.format = GL_RG;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::D24S8:
                glPixelFormat.internalFormat = GL_DEPTH24_STENCIL8;
                glPixelFormat.format = GL_DEPTH_STENCIL;
                glPixelFormat.type = GL_UNSIGNED_INT_24_8;
                break;
            case Spec::PixelFormat::RGB8:
                glPixelFormat.internalFormat = GL_RGB8;
                glPixelFormat.format = GL_RGB;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
                break;
            case Spec::PixelFormat::RGB16F:
                glPixelFormat.internalFormat = GL_RGB16F;
                glPixelFormat.format = GL_RGB;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::RGBA16F:
                glPixelFormat.internalFormat = GL_RGBA16F;
                glPixelFormat.format = GL_RGBA;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::RGB32F:
                glPixelFormat.internalFormat = GL_RGB32F;
                glPixelFormat.format = GL_RGB;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::RGBA8:
                glPixelFormat.internalFormat = GL_RGBA8;
                glPixelFormat.format = GL_RGBA;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
                break;
            case Spec::PixelFormat::RGBA32F:
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
                case Parameter::Filtering::LINEAR_MIPMAP_LINEAR:
                    return GL_LINEAR_MIPMAP_LINEAR;
                case Parameter::Filtering::NEAREST:
                    return GL_NEAREST;
                case Parameter::Filtering::NEAREST_MIPMAP_NEAREST:
                    return GL_NEAREST_MIPMAP_NEAREST;
                default:
                    return GLU_INVALID_VALUE;
                }
            };

            auto translateWrap = [](const Parameter::WrapMode wrap) {
                switch (wrap)
                {
                case Parameter::WrapMode::CLAMP_TO_EDGE:
                    return GL_CLAMP_TO_EDGE;
                case Parameter::WrapMode::NONE:
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

        char* name = nullptr;
        Spec::PixelFormat pixelFormat = Spec::PixelFormat::kInvalid;
        Parameter parameter = { };
        u32 numMips = 0;
        u8* pixelData = nullptr;
        u64 glHandle;
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
            glCreateTextures(GL_TEXTURE_2D, 1, &glObject);
            glBindTexture(GL_TEXTURE_2D, getGpuObject());
            auto glPixelFormat = translatePixelFormat(pixelFormat);
            glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, pixelData);
            glBindTexture(GL_TEXTURE_2D, 0);

            initializeTextureParameters(getGpuObject(), parameter);

            if (numMips > 1u)
            {
                glGenerateTextureMipmap(getGpuObject());
            }

#if BINDLESS_TEXTURE
            glHandle = glGetTextureHandleARB(getGpuObject());
#endif
        }

        ~Texture2DRenderable()
        {
            ITextureRenderable::~ITextureRenderable();
        }

        u32 width;
        u32 height;
    };

    struct DepthTexture2D : public Texture2DRenderable
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

        DepthTexture2D(const char* name, u32 width, u32 height)
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
        { 
#if BINDLESS_TEXTURE
            glHandle = glGetTextureHandleARB(getGpuObject());
#endif
        }

        ~DepthTexture2D()
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
            glCreateTextures(GL_TEXTURE_3D, 1, &glObject);
            glBindTexture(GL_TEXTURE_3D, getGpuObject());
            auto glPixelFormat = translatePixelFormat(pixelFormat);
            glTexImage3D(GL_TEXTURE_3D, 0, glPixelFormat.internalFormat, width, height, depth, 0, glPixelFormat.format, glPixelFormat.type, pixelData);
            glBindTexture(GL_TEXTURE_3D, 0);

            initializeTextureParameters(getGpuObject(), parameter);

            if (numMips > 1u)
            {
                glGenerateTextureMipmap(getGpuObject());
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
            glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &glObject);
            glBindTexture(GL_TEXTURE_CUBE_MAP, getGpuObject());
            auto glPixelFormat = translatePixelFormat(pixelFormat);
            for (i32 f = 0; f < 6; ++f)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, glPixelFormat.internalFormat, resolution, resolution, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
            }
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

            initializeTextureParameters(getGpuObject(), parameter);

            if (numMips > 1u)
            {
                glGenerateTextureMipmap(getGpuObject());
            }

#if BINDLESS_TEXTURE
            glHandle = glGetTextureHandleARB(getGpuObject());
#endif
        }

        ~TextureCubeRenderable()
        {
            ITextureRenderable::~ITextureRenderable();
        }

        u32 resolution;
    };
}

// custom hash function for ITextureRenderable::Spec
namespace std
{
    template <>
    struct hash<Cyan::ITextureRenderable::Spec>
    {
        std::size_t operator()(const Cyan::ITextureRenderable::Spec& spec) const
        {
            std::string key = std::to_string(spec.width) + 'x' + std::to_string(spec.height);
            switch (spec.pixelFormat)
            {
            case PF_RGB16F:
                key += "_RGB16F";
                break;
            case PF_RGB32F:
                key += "_RGB32F";
                break;
            case PF_RGBA16F:
                key += "_RGBA16F";
                break;
            case PF_RGBA32F:
                key += "_RGBA32F";
                break;
            default:
                break;
            }
            return std::hash<std::string>()(key);
        }
    };
}