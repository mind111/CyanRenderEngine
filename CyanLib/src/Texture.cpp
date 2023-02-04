#include <stbi/stb_image.h>
#include "Texture.h"
#include "AssetManager.h"

namespace Cyan
{
    u32 Texture::count = 0;
    Texture::Texture(const Format& inFormat)
        : format(inFormat)
    {
        std::string defaultName("Texture_");
        defaultName += std::to_string(count++);
    }

    Texture::Texture(const char* inName, const Format& inFormat)
        : name(inName), format(inFormat)
    {
        // todo: need to verify if name collides
    }

    void Sampler::init()
    {
        assert(!bInitialized);
        bInitialized = true;
    }

    void Sampler2D::init(Texture2D* texture)
    {
        Sampler::init();

        switch (wrapS)
        {
        case Sampler::Addressing::kClampToEdge:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            break;
        case Sampler::Addressing::kWrap:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_S, GL_REPEAT);
            break;
        default:
            assert(0);
        }

        switch (wrapT)
        {
        case Sampler::Addressing::kClampToEdge:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        case Sampler::Addressing::kWrap:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_T, GL_REPEAT);
            break;
        default:
            assert(0);
        }

        switch (minFilter)
        {
        case Sampler::Filtering::kPoint:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            break;
        case Sampler::Filtering::kBilinear:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            break;
        case Sampler::Filtering::kTrilinear:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            break;
        case Sampler::Filtering::kMipmapPoint:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            break;
        default:
            assert(0);
        }

        switch (magFilter)
        {
        case Sampler::Filtering::kPoint:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case Sampler::Filtering::kBilinear:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        default:
            assert(0);
        }
    }

    void SamplerCube::init(TextureCube* texture)
    {
        Sampler::init();

        switch (wrapS)
        {
        case Sampler::Addressing::kClampToEdge:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            break;
        case Sampler::Addressing::kWrap:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_S, GL_REPEAT);
            break;
        default:
            assert(0);
        }

        switch (wrapT)
        {
        case Sampler::Addressing::kClampToEdge:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            break;
        case Sampler::Addressing::kWrap:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_WRAP_T, GL_REPEAT);
            break;
        default:
            assert(0);
        }

        switch (minFilter)
        {
        case Sampler::Filtering::kPoint:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            break;
        case Sampler::Filtering::kBilinear:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            break;
        case Sampler::Filtering::kTrilinear:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            break;
        default:
            assert(0);
        }

        switch (magFilter)
        {
        case Sampler::Filtering::kPoint:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case Sampler::Filtering::kBilinear:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        default:
            assert(0);
        }
    }

    Texture2D::Spec::Spec(const Image& inImage, bool bGenerateMipmap)
        : Texture::Spec(TEX_2D), width(inImage.width), height(inImage.height)
    {
        switch (inImage.bitsPerChannel)
        {
        case 8:
            switch (inImage.numChannels)
            {
            case 1: format = PF_R8; break;
            case 3: format = PF_RGB8; break;
            case 4: format = PF_RGBA8; break;
            default: assert(0); break;
            }
            break;
        case 16:
            switch (inImage.numChannels)
            {
            case 3: format = PF_RGB16F; break;
            case 4: format = PF_RGBA16F; break;
            default: assert(0); break;
            }
            break;
        case 32:
            switch (inImage.numChannels)
            {
            case 3: format = PF_RGB32F; break;
            case 4: format = PF_RGBA32F; break;
            default: assert(0); break;
            }
            break;
        default:
            assert(0);
        }

        if (bGenerateMipmap)
        {
            numMips = (i32)log2(min(width, height)) + 1;
        }
    }

    Texture2D::Texture2D(const Image& inImage, bool bGenerateMipmap, const Sampler2D& inSampler)
        : Texture(Texture2D::Spec(inImage).format), sampler(inSampler)
    {
        srcImage = AssetManager::getAsset<Image>(inImage.name.c_str());
        assert(srcImage);

        Texture2D::Spec spec(inImage, bGenerateMipmap);
        width = spec.width;
        height = spec.height;
        numMips = spec.numMips;
    }

    Texture2D::Texture2D(const char* inName, const Image& inImage, bool bGenerateMipmap, const Sampler2D& inSampler)
        : Texture(inName, Texture2D::Spec(inImage).format), sampler(inSampler)
    {
        srcImage = AssetManager::getAsset<Image>(inImage.name.c_str());
        assert(srcImage);

        Texture2D::Spec spec(inImage, bGenerateMipmap);
        width = spec.width;
        height = spec.height;
        numMips = spec.numMips;
    }

    Texture2D::Texture2D(const Spec& inSpec, const Sampler2D& inSampler)
        : Texture(inSpec.format), sampler(inSampler), width(inSpec.width), height(inSpec.height), numMips(inSpec.numMips)
    {

    }

    Texture2D::Texture2D(const char* inName, const Spec& inSpec, const Sampler2D& inSampler)
        : Texture(inName, inSpec.format), sampler(inSampler), width(inSpec.width), height(inSpec.height), numMips(inSpec.numMips)
    {

    }

    struct GLTextureFormat
    {
        GLint internalFormat;
        GLenum format;
        GLenum type;
    };

    static GLTextureFormat translateTextureFormat(const Texture::Format& inFormat)
    {
        GLTextureFormat glTextureFormat = { };
        switch (inFormat)
        {
        case Texture::Format::kR8:
            glTextureFormat.internalFormat = GL_R8;
            glTextureFormat.format = GL_RED;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case Texture::Format::kD24S8:
            glTextureFormat.internalFormat = GL_DEPTH24_STENCIL8;
            glTextureFormat.format = GL_DEPTH_STENCIL;
            glTextureFormat.type = GL_UNSIGNED_INT_24_8;
            break;
        case Texture::Format::kRGB8:
            glTextureFormat.internalFormat = GL_RGB8;
            glTextureFormat.format = GL_RGB;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case Texture::Format::kRGBA8:
            glTextureFormat.internalFormat = GL_RGBA8;
            glTextureFormat.format = GL_RGBA;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case Texture::Format::kRGB16F:
            glTextureFormat.internalFormat = GL_RGB16F;
            glTextureFormat.format = GL_RGB;
            // todo: should type be GL_FLOAT or GL_HALF_FLOAT, running into issue with GL_HALF_FLOAT so using GL_FLOAT for now
            glTextureFormat.type = GL_FLOAT;
            break;
        case Texture::Format::kRGBA16F:
            glTextureFormat.internalFormat = GL_RGBA16F;
            glTextureFormat.format = GL_RGBA;
            // todo: should type be GL_FLOAT or GL_HALF_FLOAT, running into issue with GL_HALF_FLOAT so using GL_FLOAT for now
            glTextureFormat.type = GL_FLOAT;
            break;
        case Texture::Format::kRGB32F:
            glTextureFormat.internalFormat = GL_RGB32F;
            glTextureFormat.format = GL_RGB;
            glTextureFormat.type = GL_FLOAT;
            break;
        case Texture::Format::kRGBA32F:
            glTextureFormat.internalFormat = GL_RGBA32F;
            glTextureFormat.format = GL_RGBA;
            glTextureFormat.type = GL_FLOAT;
            break;
        default:
            break;
        }
        return glTextureFormat;
    }

    void Texture2D::init()
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &glObject);
        glBindTexture(GL_TEXTURE_2D, getGpuResource());
        auto glPixelFormat = translateTextureFormat(format);
        if (srcImage)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, srcImage->pixels.get());
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        sampler.init(this);

        if (numMips > 1)
        {
            glGenerateTextureMipmap(getGpuResource());
        }

#if BINDLESS_TEXTURE
        glHandle = glGetTextureHandleARB(getGpuResource());
#endif
    }

    DepthTexture2D::DepthTexture2D(const Spec& inSpec, const Sampler2D& inSampler)
        : Texture2D(Texture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, PF_D24S8), inSampler)
    {

    }

    DepthTexture2D::DepthTexture2D(const char* inName, const Spec& inSpec, const Sampler2D& inSampler)
        : Texture2D(inName, Texture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, PF_D24S8), inSampler)
    {

    }

    void DepthTexture2D::init()
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &glObject);
        glBindTexture(GL_TEXTURE_2D, getGpuResource());
        auto glPixelFormat = translateTextureFormat(format);
        glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        sampler.init(this);

#if BINDLESS_TEXTURE
        glHandle = glGetTextureHandleARB(getGpuResource());
#endif
    }

    Texture2DArray::Texture2DArray(const Spec& inSpec, const Sampler2D& inSampler)
        : Texture2D(Texture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, inSpec.format), inSampler), numLayers(inSpec.numLayers)
    {

    }

    Texture2DArray::Texture2DArray(const char* inName, const Spec& inSpec, const Sampler2D& inSampler)
        : Texture2D(inName, Texture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, inSpec.format), inSampler), numLayers(inSpec.numLayers)
    {

    }

    void Texture2DArray::init()
    {
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &glObject);
        glBindTexture(GL_TEXTURE_2D_ARRAY, getGpuResource());
        auto glPixelFormat = translateTextureFormat(format);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, glPixelFormat.internalFormat, width, height, numLayers, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

        sampler.init(this);

        if (numMips)
        {
            glGenerateTextureMipmap(getGpuResource());
        }
#if BINDLESS_TEXTURE
        glHandle = glGetTextureHandleARB(getGpuResource());
#endif
    }

    TextureCube::TextureCube(const Spec& inSpec, const SamplerCube& inSampler)
        : Texture(inSpec.format), resolution(inSpec.resolution), numMips(inSpec.numMips), sampler(inSampler)
    {

    }

    TextureCube::TextureCube(const char* inName, const Spec& inSpec, const SamplerCube& inSampler)
        : Texture(inName, inSpec.format), resolution(inSpec.resolution), numMips(inSpec.numMips), sampler(inSampler)
    {
    }

    void TextureCube::init()
    {
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &glObject);
        glBindTexture(GL_TEXTURE_CUBE_MAP, getGpuResource());
        auto glPixelFormat = translateTextureFormat(format);
        for (i32 f = 0; f < 6; ++f)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, glPixelFormat.internalFormat, resolution, resolution, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        sampler.init(this);

        if (numMips > 1u)
        {
            glGenerateTextureMipmap(getGpuResource());
        }
    }

}
