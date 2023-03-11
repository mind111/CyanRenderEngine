#include <stbi/stb_image.h>

#include "Texture.h"
#include "CyanEngine.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "GfxContext.h"

namespace Cyan
{
    GfxTexture::GfxTexture(const Format& inFormat)
        : format(inFormat)
    {
    }

    void GfxTexture::bind(GfxContext* ctx, u32 inTextureUnit)
    {
        ctx->setTexture(this, inTextureUnit);
        textureUnit = inTextureUnit;
    }

    void GfxTexture::unbind(GfxContext* ctx) 
    {
        ctx->setTexture(nullptr, textureUnit);
        textureUnit = -1;
    }

    bool GfxTexture::isBound()
    {
        return (textureUnit >= 0);
    }

    void Sampler::init()
    {
        assert(!bInitialized);
        bInitialized = true;
    }

    void Sampler2D::init(GfxTexture2D* texture)
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
        case Sampler::Filtering::kMipmapPoint:
            glTextureParameteri(texture->getGpuResource(), GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
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

    GfxTexture2D::Spec::Spec(const Image& inImage, bool bGenerateMipmap)
        : GfxTexture::Spec(TEX_2D), width(inImage.width), height(inImage.height)
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

#if 0
    GfxTexture2D::GfxTexture2D(Image* inImage, bool bGenerateMipmap, const Sampler2D& inSampler)
        : GfxTexture(GfxTexture2D::Spec(*inImage).format)
        , Image::Listener(inImage)
        , srcImage(inImage)
        , sampler(inSampler) 
    {
        assert(srcImage);

        GfxTexture2D::Spec spec(*inImage, bGenerateMipmap);
        width = spec.width;
        height = spec.height;
        numMips = spec.numMips;
    }

    GfxTexture2D::GfxTexture2D(const char* inName, Image* inImage, bool bGenerateMipmap, const Sampler2D& inSampler)
        : GfxTexture(inName, GfxTexture2D::Spec(*inImage).format)
        , Image::Listener(inImage)
        , srcImage(inImage)
        , sampler(inSampler)
    {
        assert(srcImage);

        if (srcImage->state == Asset::State::kLoaded) 
        {
            GfxTexture2D::Spec spec(*inImage, bGenerateMipmap);
            width = spec.width;
            height = spec.height;
            numMips = spec.numMips;
        }
    }
#endif

    GfxTexture2D::GfxTexture2D(u8* inPixelData, const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture(inSpec.format), sampler(inSampler), width(inSpec.width), height(inSpec.height), numMips(inSpec.numMips), pixelData(inPixelData)
    {
    }

    GfxTexture2D::GfxTexture2D(const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture(inSpec.format), sampler(inSampler), width(inSpec.width), height(inSpec.height), numMips(inSpec.numMips)
    {
    }

#if 0
    GfxTexture2D::GfxTexture2D(const char* inName, const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture(inName, inSpec.format)
        , Image::Listener(nullptr)
        , sampler(inSampler)
        , width(inSpec.width)
        , height(inSpec.height)
        , numMips(inSpec.numMips)
    {

    }
#endif
   
    struct GLTextureFormat
    {
        GLint internalFormat;
        GLenum format;
        GLenum type;
    };

    static GLTextureFormat translateTextureFormat(const GfxTexture::Format& inFormat)
    {
        GLTextureFormat glTextureFormat = { };
        switch (inFormat)
        {
        case GfxTexture::Format::kR8:
            glTextureFormat.internalFormat = GL_R8;
            glTextureFormat.format = GL_RED;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case GfxTexture::Format::kD24S8:
            glTextureFormat.internalFormat = GL_DEPTH24_STENCIL8;
            glTextureFormat.format = GL_DEPTH_STENCIL;
            glTextureFormat.type = GL_UNSIGNED_INT_24_8;
            break;
        case GfxTexture::Format::kRGB8:
            glTextureFormat.internalFormat = GL_RGB8;
            glTextureFormat.format = GL_RGB;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case GfxTexture::Format::kRGBA8:
            glTextureFormat.internalFormat = GL_RGBA8;
            glTextureFormat.format = GL_RGBA;
            glTextureFormat.type = GL_UNSIGNED_BYTE;
            break;
        case GfxTexture::Format::kRGB16F:
            glTextureFormat.internalFormat = GL_RGB16F;
            glTextureFormat.format = GL_RGB;
            // todo: should type be GL_FLOAT or GL_HALF_FLOAT, running into issue with GL_HALF_FLOAT so using GL_FLOAT for now
            glTextureFormat.type = GL_FLOAT;
            break;
        case GfxTexture::Format::kRGBA16F:
            glTextureFormat.internalFormat = GL_RGBA16F;
            glTextureFormat.format = GL_RGBA;
            // todo: should type be GL_FLOAT or GL_HALF_FLOAT, running into issue with GL_HALF_FLOAT so using GL_FLOAT for now
            glTextureFormat.type = GL_FLOAT;
            break;
        case GfxTexture::Format::kRGB32F:
            glTextureFormat.internalFormat = GL_RGB32F;
            glTextureFormat.format = GL_RGB;
            glTextureFormat.type = GL_FLOAT;
            break;
        case GfxTexture::Format::kRGBA32F:
            glTextureFormat.internalFormat = GL_RGBA32F;
            glTextureFormat.format = GL_RGBA;
            glTextureFormat.type = GL_FLOAT;
            break;
        default:
            assert(0);
            break;
        }
        return glTextureFormat;
    }

    void GfxTexture2D::init()
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &glObject);
        glBindTexture(GL_TEXTURE_2D, getGpuResource());
        auto glPixelFormat = translateTextureFormat(format);
        glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, pixelData);
        glBindTexture(GL_TEXTURE_2D, 0);

        sampler.init(this);

        if (numMips > 1)
        {
            glGenerateTextureMipmap(getGpuResource());
        }
    }

    Texture2DBase::Texture2DBase(const char* inName, Image* inImage, const Sampler2D& inSampler)
        : Asset(inName)
        , Image::Listener(inImage)
        , srcImage(inImage)
        , sampler(inSampler)
    {
    }

    void Texture2DBase::import()
    {
        srcImage->import();
    }

    void Texture2DBase::load()
    {

    }

    void Texture2DBase::onLoaded()
    {
        width = srcImage->width;
        height = srcImage->height;
        switch (sampler.minFilter)
        {
        case Sampler2D::Filtering::kTrilinear:
        case Sampler2D::Filtering::kMipmapPoint:
            numMips = max(1, std::log2(min(srcImage->width, srcImage->height)));
            break;
        default:
            break;
        }
        state = State::kLoaded;
    }

    void Texture2DBase::unload()
    {
    }

    void Texture2DBase::onImageLoaded()
    {
        onLoaded();

        if (isMainThread())
        {
            // if we are on main thread, then immediately initialize gfx resources
            initGfxResource();
        }
        else
        {
            AssetManager::deferredInitAsset(this, [this](Asset* asset) {
                initGfxResource();
            });
        }
    }

    Texture2D::Texture2D(const char* inName, Image* inImage, const Sampler2D& inSampler)
        : Texture2DBase(inName, inImage, inSampler)
    {
        if (srcImage->isLoaded())
        {
            onImageLoaded();
        }
    }

    void Texture2D::initGfxResource()
    {
        GfxTexture2D::Spec spec(*srcImage, (numMips > 1));
        assert(gfxTexture == nullptr);
        gfxTexture = std::shared_ptr<GfxTexture2D>(GfxTexture2D::create(srcImage->pixels.get(), spec, sampler));
        state = State::kInitialized;
    }

#if 0
    GfxTexture2DBindless::GfxTexture2DBindless(Image* inImage, bool bGenerateMipmap, const Sampler2D& inSampler)
        : GfxTexture2D(inImage, bGenerateMipmap, inSampler)
    {

    }

    GfxTexture2DBindless::GfxTexture2DBindless(const char* inName, Image* inImage, bool bGenerateMipmap, const Sampler2D& inSampler)
        : GfxTexture2D(inName, inImage, bGenerateMipmap, inSampler)
    {

    }
#endif

    GfxTexture2DBindless::GfxTexture2DBindless(const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture2D(inSpec, inSampler)
    {
    }

    GfxTexture2DBindless::GfxTexture2DBindless(u8* inPixelData, const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture2D(inPixelData, inSpec, inSampler)
    {
    }

    Texture2DBindless::Texture2DBindless(const char* inName, Image* inImage, const Sampler2D& inSampler)
        : Texture2DBase(inName, inImage, inSampler)
    {
        if (srcImage->isLoaded())
        {
            onImageLoaded();
        }
    }

    void Texture2DBindless::initGfxResource()
    {
        GfxTexture2D::Spec spec(*srcImage, (numMips > 1));
        assert(gfxTexture == nullptr);
        gfxTexture = std::shared_ptr<GfxTexture2DBindless>(GfxTexture2DBindless::create(srcImage->pixels.get(), spec, sampler));
        state = State::kInitialized;
    }

    void GfxTexture2DBindless::init() 
    {
        GfxTexture2D::init();
#if BINDLESS_TEXTURE
        glHandle = glGetTextureHandleARB(getGpuResource());
#endif
    }

    GfxDepthTexture2D::GfxDepthTexture2D(const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture2D(GfxTexture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, PF_D24S8), inSampler)
    {
    }

#if 0
    GfxDepthTexture2D::GfxDepthTexture2D(const char* inName, const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture2D(inName, GfxTexture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, PF_D24S8), inSampler)
    {

    }
#endif

    void GfxDepthTexture2D::init()
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &glObject);
        glBindTexture(GL_TEXTURE_2D, getGpuResource());
        auto glPixelFormat = translateTextureFormat(format);
        glTexImage2D(GL_TEXTURE_2D, 0, glPixelFormat.internalFormat, width, height, 0, glPixelFormat.format, glPixelFormat.type, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        sampler.init(this);
    }

    GfxDepthTexture2DBindless::GfxDepthTexture2DBindless(const Spec& inSpec, const Sampler2D& inSampler)
        : GfxDepthTexture2D(inSpec, inSampler)
    {
    }

#if 0
    GfxDepthTexture2DBindless::GfxDepthTexture2DBindless(const char* inName, const Spec& inSpec, const Sampler2D& inSampler)
        : GfxDepthTexture2D(inName, inSpec, inSampler)
    {

    }
#endif

    void GfxDepthTexture2DBindless::init()
    {
        GfxDepthTexture2D::init();
#if BINDLESS_TEXTURE
        glHandle = glGetTextureHandleARB(getGpuResource());
#endif
    }

    void GfxDepthTexture2DBindless::makeResident()
    {
        if (glIsTextureHandleResidentARB(getTextureHandle()) == GL_FALSE)
        {
            glMakeTextureHandleResidentARB(getTextureHandle());
        }
    }

    GfxTexture2DArray::GfxTexture2DArray(const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture2D(GfxTexture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, inSpec.format), inSampler), numLayers(inSpec.numLayers)
    {
    }

#if 0
    GfxTexture2DArray::GfxTexture2DArray(const char* inName, const Spec& inSpec, const Sampler2D& inSampler)
        : GfxTexture2D(inName, GfxTexture2D::Spec(inSpec.width, inSpec.height, inSpec.numMips, inSpec.format), inSampler), numLayers(inSpec.numLayers)
    {

    }
#endif

    void GfxTexture2DArray::init()
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
    }

    TextureCube::TextureCube(const Spec& inSpec, const SamplerCube& inSampler)
        : GfxTexture(inSpec.format), resolution(inSpec.resolution), numMips(inSpec.numMips), sampler(inSampler)
    {
    }

#if 0
    TextureCube::TextureCube(const char* inName, const Spec& inSpec, const SamplerCube& inSampler)
        : GfxTexture(inName, inSpec.format), resolution(inSpec.resolution), numMips(inSpec.numMips), sampler(inSampler)
    {
    }
#endif

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
