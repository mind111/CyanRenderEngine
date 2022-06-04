#pragma once

#include <string>

#include "glew.h"

#include "Common.h"
#include "CyanCore.h"
#include "Asset.h"

namespace Cyan
{
#if 0
    // TODO: how to handle texture name?
    struct TextureRenderable : public GpuResource
    {
        enum class Type
        {
            TEX_2D = 0,
            TEX_2D_ARRAY,
            TEX_3D,
            TEX_CUBEMAP
        };

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
        
        enum class ColorFormat
        {
            R16F = 0,
            R32F,
            R32G32F,
            Lum32F,
            R8G8B8,
            R16G16B16,
            R8G8B8A8,
            R16G16B16A16,
            R32G32B32,
            R32G32B32A32,
            D24S8
        };

        enum class DataType
        {
            UNSIGNED_BYTE,
            UNSIGNED_INT,
            UNSIGNED_INT_24_8,
            Float,
        };

        std::string name;
        void* data;
        ColorFormat format;
        DataType dataType;
        Filtering minFilter;
        Filtering magFilter;
        Wrap wrapS;
        Wrap wrapT;
        Wrap wrapR;
        Type type;
        u32 width;
        u32 height;
        u32 depth;
        GLuint handle;
    };
#endif
    struct ITextureRenderable : public Asset, public GpuResource
    {
        struct Spec
        {
            enum class Type
            {
                kTex2D,
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

            u32 width = 0;
            u32 height = 0;
            u32 depth = 0;
            u32 numMips = 0;
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

        ITextureRenderable(const char* inName, const Spec& inSpec, Parameter inParams = Parameter{ })
            : GpuResource(),
            name(inName),
            pixelFormat(inSpec.pixelFormat),
            parameter(inParams),
            numMips(inSpec.numMips),
            pixelData(inSpec.pixelData)
        {

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
                glPixelFormat.internalFormat = GL_LUMINANCE;
                glPixelFormat.format = GL_LUMINANCE8;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
                break;
            case Spec::PixelFormat::R16F:
                glPixelFormat.internalFormat = GL_R;
                glPixelFormat.format = GL_R16F;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R32F:
                glPixelFormat.internalFormat = GL_R;
                glPixelFormat.format = GL_R32F;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::D24S8:
                glPixelFormat.internalFormat = GL_DEPTH_STENCIL;
                glPixelFormat.format = GL_DEPTH24_STENCIL8;
                glPixelFormat.type = GL_UNSIGNED_INT_24_8;
                break;
            case Spec::PixelFormat::R8G8B8:
                glPixelFormat.internalFormat = GL_RGB;
                glPixelFormat.format = GL_RGB8UI;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
            case Spec::PixelFormat::R16G16B16:
                glPixelFormat.internalFormat = GL_RGB;
                glPixelFormat.format = GL_RGB16F;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R32G32B32:
                glPixelFormat.internalFormat = GL_RGB;
                glPixelFormat.format = GL_RGB32F;
                glPixelFormat.type = GL_FLOAT;
                break;
            case Spec::PixelFormat::R8G8B8A8:
                glPixelFormat.internalFormat = GL_RGBA;
                glPixelFormat.format = GL_RGBA8;
                glPixelFormat.type = GL_UNSIGNED_BYTE;
                break;
            case Spec::PixelFormat::R32G32B32A32:
                glPixelFormat.internalFormat = GL_RGBA;
                glPixelFormat.format = GL_RGBA32F;
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

        u32 width;
        u32 height;
    };

    struct Texture3DRenderable : public ITextureRenderable
    {
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

        u32 width;
        u32 height;
        u32 depth;
    };

    struct TextureCubeRenderable : public ITextureRenderable
    {
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

        u32 resolution;
    };

    struct TextureSpec
    {
        TextureRenderable::Type type;
        u32 width, height, depth;
        u32 numMips;
        TextureRenderable::ColorFormat format;
        TextureRenderable::DataType dataType;
        TextureRenderable::Filtering min;
        TextureRenderable::Filtering mag;
        TextureRenderable::Wrap s;
        TextureRenderable::Wrap t;
        TextureRenderable::Wrap r;
        void* data;
    };

    class TextureManager
    {
    public:
        TextureManager();
        ~TextureManager() { }
        static TextureManager* get();
        u32 getNumTextures();
        TextureRenderable* getTexture(const char* name);
        void addTexture(TextureRenderable* texture);
        TextureRenderable* createTexture(const char* name, TextureSpec spec);
        TextureRenderable* createTextureHDR(const char* name, TextureSpec spec);
        TextureRenderable* createTexture(const char* name, const char* file, TextureSpec& spec);
        TextureRenderable* createTextureHDR(const char* name, const char* file, TextureSpec& spec);
        TextureRenderable* createTexture3D(const char* name, TextureSpec spec);
        TextureRenderable* createArrayTexture2D(const char* name, TextureSpec& spec);
        TextureRenderable* createDepthTexture(const char* name, u32 width, u32 height);
        void copyTexture(TextureRenderable* dst, TextureRenderable* src);

        static std::vector<TextureRenderable*> s_textures;
        static TextureManager* m_singleton;
    };
}