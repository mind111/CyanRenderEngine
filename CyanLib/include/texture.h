#pragma once

#include <string>
#include <memory>

#include <glew/glew.h>

#include "Common.h"
#include "CyanCore.h"
#include "Asset.h"

/**
* convenience macros
*/
#define TEX_2D Cyan::ITexture::Spec::Type::kTex2D
#define TEX_2D_ARRAY Cyan::ITexture::Spec::Type::kTex2DArray
#define TEX_DEPTH Cyan::ITexture::Spec::Type::kDepthTex
#define TEX_3D Cyan::ITexture::Spec::Type::kTex3D
#define TEX_CUBE Cyan::ITexture::Spec::Type::kTexCube

// WM stands for "wrap mode"
#define WM_CLAMP Cyan::ITexture::Parameter::WrapMode::CLAMP_TO_EDGE
#define WM_WRAP Cyan::ITexture::Parameter::WrapMode::WRAP
// FM stands for "filter mode"
#define FM_POINT Cyan::ITexture::Parameter::Filtering::NEAREST
#define FM_BILINEAR Cyan::ITexture::Parameter::Filtering::LINEAR
#define FM_TRILINEAR Cyan::ITexture::Parameter::Filtering::LINEAR_MIPMAP_LINEAR
#define FM_MIPMAP_POINT Cyan::ITexture::Parameter::Filtering::NEAREST_MIPMAP_NEAREST
// PF stands for "pixel format" 
#define PF_R8 Cyan::ITexture::Spec::PixelFormat::R8
#define PF_R8UI Cyan::ITexture::Spec::PixelFormat::R8UI
#define PF_RGB8 Cyan::ITexture::Spec::PixelFormat::RGB8
#define PF_RGBA8 Cyan::ITexture::Spec::PixelFormat::RGBA8
#define PF_R16F Cyan::ITexture::Spec::PixelFormat::R16F
#define PF_RGB16F Cyan::ITexture::Spec::PixelFormat::RGB16F
#define PF_RGBA16F Cyan::ITexture::Spec::PixelFormat::RGBA16F
#define PF_RGB32F Cyan::ITexture::Spec::PixelFormat::RGB32F
#define PF_RGBA32F Cyan::ITexture::Spec::PixelFormat::RGBA32F

namespace Cyan
{
    struct Image
    {
        std::string name;
        i32 width;
        i32 height;
        i32 numChannels;
        i32 bitsPerChannel;
        std::shared_ptr<u8> pixels = nullptr;
    };

    // todo: rework Texture classes....!!!
    struct ITexture : public Asset, public GpuObject
    {
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("ITexture");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("ITexture"); 
        }

        struct Spec
        {
            enum class Type
            {
                kTex2D,
                kTex2DArray,
                kDepthTex,
                kTex3D,
                kTexCube,
                kCount
            };

            enum class PixelFormat
            {
                R8 = 0,
                R8UI,
                R16F,
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

        ITexture(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ });
        ITexture(const Spec& inSpec, Parameter inParams);

        virtual ~ITexture() 
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
            case Spec::PixelFormat::R8:    
                glPixelFormat.internalFormat = GL_R8I;
                glPixelFormat.format = GL_RED;
                glPixelFormat.type = GL_BYTE;
                break;
            case Spec::PixelFormat::R8UI:
                glPixelFormat.internalFormat = GL_R8;
                glPixelFormat.format = GL_RED;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
                break;
            case Spec::PixelFormat::Lum32F:
                glPixelFormat.internalFormat = GL_LUMINANCE8;
                glPixelFormat.format = GL_LUMINANCE;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R16F:
                glPixelFormat.internalFormat = GL_R16F;
                glPixelFormat.format = GL_RED;
                glPixelFormat.type = GL_HALF_FLOAT;
                break;
            case Spec::PixelFormat::R32F:
                glPixelFormat.internalFormat = GL_R32F;
                glPixelFormat.format = GL_R;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::RG16F:
                glPixelFormat.internalFormat = GL_RG16F;
                glPixelFormat.format = GL_RG;
                glPixelFormat.type = GL_HALF_FLOAT;
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
                glPixelFormat.type = GL_HALF_FLOAT;
                break;
            case Spec::PixelFormat::RGBA16F:
                glPixelFormat.internalFormat = GL_RGBA16F;
                glPixelFormat.format = GL_RGBA;
                glPixelFormat.type = GL_HALF_FLOAT;
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

        static u32 count;
        char* name = nullptr;
        Spec::PixelFormat pixelFormat = Spec::PixelFormat::kInvalid;
        Parameter parameter = { };
        u32 numMips = 0;
        u8* pixelData = nullptr;
        u64 glHandle;
    };

    struct Texture2D : public ITexture
    {
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("Texture2D");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("Texture2D"); 
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

        Texture2D(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ });
        Texture2D(const Spec& inSpec, Parameter inParams = Parameter{ });
        Texture2D(const Image& inImage, Parameter inParams = Parameter{ });

        ~Texture2D()
        {
        }

        void initGpuResource(const Image& srcImage)
        {
        }

        u32 width;
        u32 height;
        bool bInitialized = false;
    };

    struct Texture2DArray : public ITexture
    {
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("Texture2DArray");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("Texture2DArray"); 
        }

        virtual Spec getTextureSpec() override
        {
            return Spec {
                width, /* width */
                height, /* height */
                depth, /* depth */
                numMips, /* numMips */
                Spec::Type::kTex2D, /* texture type */
                pixelFormat, /* pixel format */
                pixelData /* pixel data */
            };
        }

        Texture2DArray(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : ITexture(inName, inSpec, inParams),
            width(inSpec.width),
            height(inSpec.height),
            depth(inSpec.depth)
        {
            glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glObject);
            glBindTexture(GL_TEXTURE_2D_ARRAY, getGpuObject());
            auto glPixelFormat = translatePixelFormat(pixelFormat);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, glPixelFormat.internalFormat, width, height, depth, 0, glPixelFormat.format, glPixelFormat.type, pixelData);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

            initializeTextureParameters(getGpuObject(), parameter);

            if (numMips > 1u)
            {
                glGenerateTextureMipmap(getGpuObject());
            }

#if BINDLESS_TEXTURE
            glHandle = glGetTextureHandleARB(getGpuObject());
#endif
        }

        u32 width;
        u32 height;
        u32 depth;
    };

    struct DepthTexture2D : public Texture2D
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

        /* ITexture interface */
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
            : Texture2D(
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
            ITexture::~ITexture();
        }
    };

    struct Texture3D : public ITexture
    {
        /* Asset interface */
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("Texture3D");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("Texture3D"); 
        }

        /* ITexture interface */
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

        Texture3D(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : ITexture(inName, inSpec, inParams),
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

        ~Texture3D()
        {
            ITexture::~ITexture();
        }

        u32 width;
        u32 height;
        u32 depth;
    };

    struct TextureCube : public ITexture
    {
        /* Asset interface */
        virtual std::string getAssetObjectTypeDesc() override
        {
            return std::string("TextureCube");
        }

        static std::string getAssetClassTypeDesc() 
        { 
            return std::string("TextureCube"); 
        }

        /* ITexture interface */
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

        TextureCube(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : ITexture(inName, inSpec, inParams),
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

        ~TextureCube()
        {
            ITexture::~ITexture();
        }

        u32 resolution;
    };
}

// custom hash function for ITexture::Spec
namespace std
{
    template <>
    struct hash<Cyan::ITexture::Spec>
    {
        std::size_t operator()(const Cyan::ITexture::Spec& spec) const
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