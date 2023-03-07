#pragma once

#include <cassert>
#include <string>
#include <memory>

#include <glew/glew.h>

#include "Common.h"
#include "CyanCore.h"
#include "Asset.h"
#include "Image.h"

#define BINDLESS_TEXTURE 1

/**
* convenience macros
*/
#define TEX_2D Cyan::GfxTexture::Type::kTex2D
#define TEX_2D_ARRAY Cyan::GfxTexture::Type::kTex2DArray
#define TEX_DEPTH Cyan::GfxTexture::Type::kDepth2D
#define TEX_3D Cyan::GfxTexture::Type::kTex3D
#define TEX_CUBE Cyan::GfxTexture::Type::kTexCube

// WM stands for "wrap mode"
#define WM_CLAMP Cyan::Sampler::Addressing::kClampToEdge
#define WM_WRAP Cyan::Sampler::Addressing::kWrap

// FM stands for "filter mode"
#define FM_POINT Cyan::Sampler::Filtering::kPoint
#define FM_MIPMAP_POINT Cyan::Sampler::Filtering::kMipmapPoint
#define FM_BILINEAR Cyan::Sampler::Filtering::kBilinear
#define FM_TRILINEAR Cyan::Sampler::Filtering::kTrilinear

// PF stands for "pixel format" 
#define PF_R8 Cyan::GfxTexture::Format::kR8
#define PF_RGB8 Cyan::GfxTexture::Format::kRGB8
#define PF_RGBA8 Cyan::GfxTexture::Format::kRGBA8
#define PF_R16F Cyan::GfxTexture::Format::kR16F
#define PF_RGB16F Cyan::GfxTexture::Format::kRGB16F
#define PF_RGBA16F Cyan::GfxTexture::Format::kRGBA16F
#define PF_D24S8 Cyan::GfxTexture::Format::kD24S8
#define PF_RGB32F Cyan::GfxTexture::Format::kRGB32F
#define PF_RGBA32F Cyan::GfxTexture::Format::kRGBA32F

namespace Cyan
{
    /**
     * Encapsulate a gl texture
     */
    struct GfxTexture : public GpuResource
    {
        enum class Type : u32
        {
            kTex2D = 0,
            kDepth2D,
            kTex2DArray,
            kTexCube,
            kTex3D,
            kCount
        };

        enum class Format : u32
        {
            kR8 = 0,
            kRGB8,
            kRGBA8,
            kRGB16F,
            kRGBA16F,
            kD24S8,
            kRGB32F,
            kRGBA32F,
            kCount
        };

        struct Spec
        {
            Spec(Type inType)
                : type(inType)
            {
            }

            Spec(Type inType, Format inFormat)
                : type(inType), format(inFormat)
            {
            }

            Type type = Type::kCount;
            Format format = Format::kCount;
        };

        virtual bool operator==(const GfxTexture& rhs) const = 0;
        virtual ~GfxTexture() { }

        // static u32 count;
        // std::string name;
        Format format;
    protected:
        GfxTexture(const Format& inFormat);
        virtual void init() = 0;
    };

    struct Sampler
    {
        enum class Addressing
        {
            kClampToEdge = 0,
            kWrap,
            kCount
        };

        enum class Filtering
        {
            kPoint = 0,
            kMipmapPoint,
            kBilinear,
            kTrilinear,
            kCount
        };

        void init();

        bool bInitialized = false;
    };

    // samplers
    struct GfxTexture2D;
    struct GfxTexture2DArray;
    struct TextureCube;

    struct Sampler2D : public Sampler
    {
        Sampler2D(const Sampler::Filtering& inMinFilter = FM_POINT, const Sampler::Filtering& inMagFilter = FM_POINT, const Sampler::Addressing& inWrapS = WM_CLAMP, const Sampler::Addressing& inWrapT = WM_CLAMP) 
            : minFilter(inMinFilter), magFilter(inMagFilter), wrapS(inWrapS), wrapT(inWrapT)
        { 
        }

        void init(GfxTexture2D* texture);

        Filtering minFilter = Filtering::kPoint;
        Filtering magFilter = Filtering::kPoint;
        Addressing wrapS = Addressing::kClampToEdge;
        Addressing wrapT = Addressing::kClampToEdge;
    };

    struct Sampler3D : public Sampler
    {
    };

    struct SamplerCube : public Sampler
    {
        void init(TextureCube* texture);

        Addressing wrapS = Addressing::kClampToEdge;
        Addressing wrapT = Addressing::kClampToEdge;
        Filtering minFilter = Filtering::kPoint;
        Filtering magFilter = Filtering::kPoint;
    };

    // todo: decouple texture assets from runtime textures, where they will be used for rendering at runtime, 
    // while these normal texture class will inherit from Asset, so they can be serialized / de-serialized
    struct GfxTexture2D : public GfxTexture
    {
        struct Spec : public GfxTexture::Spec
        {
            Spec()
                : GfxTexture::Spec(TEX_2D)
            {
            }

            Spec(const Type& inType)
                : GfxTexture::Spec(inType)
            {
            }

            Spec(const Type& inType, const Format& inFormat)
                : GfxTexture::Spec(inType, inFormat)
            {
            }

            Spec(const Spec& srcSpec)
                : GfxTexture::Spec(TEX_2D, srcSpec.format), width(srcSpec.width), height(srcSpec.height), numMips(srcSpec.numMips)
            {
            }

            Spec(u32 inWidth, u32 inHeight, u32 inNumMips, GfxTexture::Format inFormat)
                : GfxTexture::Spec(TEX_2D, inFormat), width(inWidth), height(inHeight), numMips(inNumMips)
            {
            }

            Spec(const Type& inType, u32 inWidth, u32 inHeight, u32 inNumMips, GfxTexture::Format inFormat)
                : GfxTexture::Spec(inType, inFormat), width(inWidth), height(inHeight), numMips(inNumMips)
            {
            }

            Spec(const Image& inImage, bool bGenerateMipmap = true);

            bool operator==(const Spec& rhs) const
            {
                return (rhs.width == width) && (rhs.height == height) && (rhs.numMips == numMips) && (rhs.format == format) && (rhs.type == type);
            }

            u32 width = 0;
            u32 height = 0;
            u32 numMips = 1;
        };

        static GfxTexture2D* create(u8* inPixelData, const Spec& inSpec, const Sampler2D& inSampler = { })
        {
            auto outTexture = new GfxTexture2D(inPixelData, inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        static GfxTexture2D* create(const Spec& inSpec, const Sampler2D& inSampler = { })
        {
            auto outTexture = new GfxTexture2D(inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        Spec getSpec() const
        {
            return Spec(width, height, numMips, format);
        }

        virtual bool operator==(const GfxTexture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const GfxTexture2D*>(&rhs))
            {
                return ptr->getSpec() == getSpec();
            }
            return false;
        }

        Sampler2D sampler;
        u32 width = 0;
        u32 height = 0;
        u32 numMips = 1;
        u8* pixelData = nullptr;
    protected:
        /**
         * Hiding the constructors to force creating these object throught the static create() method, because
         */
        GfxTexture2D(u8* inPixelData, const Spec& inSpec, const Sampler2D& inSampler = {});
        GfxTexture2D(const Spec& inSpec, const Sampler2D& inSampler = {});

        /* GfxTexture interface */
        virtual void init() override;
    };

    struct Texture2DBase : public Asset, Image::Listener 
    {
        Texture2DBase(const char* inName, Image* inImage, const Sampler2D& inSampler2D);
        virtual ~Texture2DBase() { }

        /* Asset Interface */
        virtual void import() override;
        virtual void load() override;
        virtual void onLoaded() override;
        virtual void unload() override;

        /* Image::Listener Interface */
        virtual bool operator==(const Image::Listener& rhs) override
        {
            if (const Texture2DBase* texture = dynamic_cast<const Texture2DBase*>(&rhs))
            {
                return name == texture->name;
            }
            return false;
        }

        virtual void onImageLoaded() override;

        virtual void initGfxResource() = 0;
        virtual GfxTexture2D* getGfxResource() = 0;

        Image* srcImage = nullptr;
        Sampler2D sampler;
        i32 width = -1;
        i32 height = -1;
        i32 numMips = 1;
    };

    struct Texture2D : public Texture2DBase
    {
        Texture2D(const char* inName, Image* inImage, const Sampler2D& inSampler2D);
        virtual ~Texture2D() { }

        /* Texture2DBase interface */
        virtual void initGfxResource() override;
        virtual GfxTexture2D* getGfxResource() override { return gfxTexture.get(); };

        // gfx resource
        std::shared_ptr<GfxTexture2D> gfxTexture = nullptr;
    };

    struct GfxTexture2DBindless : public GfxTexture2D
    {
        static GfxTexture2DBindless* create(u8* inPixelData, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D())
        {
            auto outTexture = new GfxTexture2DBindless(inPixelData, inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        static GfxTexture2DBindless* create(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D())
        {
            auto outTexture = new GfxTexture2DBindless(inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

#if BINDLESS_TEXTURE
        u64 glHandle;
#endif
    protected:
        GfxTexture2DBindless(u8* inPixelData, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        GfxTexture2DBindless(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());

        virtual void init() override;
    };

    struct Texture2DBindless : public Texture2DBase
    {
        Texture2DBindless(const char* inName, Image* inImage, const Sampler2D& inSampler2D);
        virtual ~Texture2DBindless() { }

        /* Texture2DBase interface */
        virtual void initGfxResource() override;
        virtual GfxTexture2D* getGfxResource() override { return gfxTexture.get(); };

        u64 getTextureHandle() { return gfxTexture->glHandle; }

        // gfx resource
        std::shared_ptr<GfxTexture2DBindless> gfxTexture = nullptr;
    };

    struct GfxDepthTexture2D : public GfxTexture2D
    {
        struct Spec : public GfxTexture2D::Spec 
        {
            Spec()
                : GfxTexture2D::Spec(TEX_DEPTH, PF_D24S8)
            {
            }

            Spec(const Spec& srcSpec)
                : GfxTexture2D::Spec(TEX_DEPTH, srcSpec.width, srcSpec.height, srcSpec.numMips, PF_D24S8)
            {
            }

            Spec(u32 inWidth, u32 inHeight, u32 inNumMips)
                : GfxTexture2D::Spec(TEX_DEPTH, inWidth, inHeight, inNumMips, PF_D24S8)
            {
            }

            bool operator==(const Spec& rhs) const
            {
                return (rhs.width == width) && (rhs.height == height) && (rhs.numMips == numMips) && (rhs.format == format) && (rhs.type == type);
            }
        };

        static GfxDepthTexture2D* create(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D())
        {
            auto outTexture = new GfxDepthTexture2D(inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        Spec getSpec() const
        {
            return Spec(width, height, numMips);
        }

        virtual bool operator==(const GfxTexture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const GfxDepthTexture2D*>(&rhs))
            {
                return ptr->getSpec() == getSpec();
            }
            return false;
        }
    protected:
        GfxDepthTexture2D(const GfxDepthTexture2D::Spec& inSpec, const Sampler2D& inSampler = Sampler2D());

        virtual void init() override;
    };

    struct GfxDepthTexture2DBindless : public GfxDepthTexture2D
    {
        static GfxDepthTexture2DBindless* create(const GfxDepthTexture2D::Spec& inSpec, const Sampler2D& inSampler = Sampler2D())
        {
            auto outTexture = new GfxDepthTexture2DBindless(inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        u64 getTextureHandle() { return glHandle; }
        void makeResident();

    protected:
        GfxDepthTexture2DBindless(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        virtual void init() override;

    private:
#if BINDLESS_TEXTURE
        u64 glHandle;
#endif
    };

    struct GfxTexture2DArray : public GfxTexture2D
    {
        struct Spec : public GfxTexture2D::Spec
        {
            Spec()
                : GfxTexture2D::Spec(TEX_2D_ARRAY)
            {
            }

            Spec(const Spec& srcSpec)
                : GfxTexture2D::Spec(TEX_2D_ARRAY, srcSpec.width, srcSpec.height, srcSpec.numMips, srcSpec.format), numLayers(srcSpec.numLayers)
            {
            }

            Spec(u32 inWidth, u32 inHeight, u32 inNumMips, u32 inNumLayers, GfxTexture::Format inFormat)
                : GfxTexture2D::Spec(TEX_2D_ARRAY, inWidth, inHeight, inNumMips, inFormat), numLayers(inNumLayers)
            {
            }

            bool operator==(const Spec& rhs) const
            {
                return (rhs.width == width) && (rhs.height == height) && (rhs.numLayers == numLayers) && (rhs.numMips == numMips) && (rhs.format == format) && (rhs.type == type);
            }

            u32 numLayers = 1;
        };

        static GfxTexture2DArray* create(const GfxTexture2DArray::Spec& inSpec, const Sampler2D& inSampler = Sampler2D())
        {
            auto outTexture = new GfxTexture2DArray(inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        Spec getSpec() const
        {
            return Spec(width, height, numMips, numLayers, format);
        }

        virtual bool operator==(const GfxTexture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const GfxTexture2DArray*>(&rhs))
            {
                return ptr->getSpec() == getSpec();
            }
            return false;
        }

        u32 numLayers = 1;

    protected:
        /* GfxTexture interface */
        virtual void init() override;

        GfxTexture2DArray(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        // GfxTexture2DArray(const char* inName, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
    };

#if 0
    struct Texture3D : public Texture
    {
        struct Spec : public Texture::Spec
        {
        };

        virtual void init();

        Sampler3D sampler;
        u32 width = 0;
        u32 height = 0;
        u32 depth = 1;
        u32 numMips = 1;
    };
#endif

    struct TextureCube : public GfxTexture
    {
        struct Spec : public GfxTexture::Spec
        {
            Spec()
                : GfxTexture::Spec(TEX_CUBE)
            {

            }

            Spec(const Spec& srcSpec)
                : GfxTexture::Spec(TEX_CUBE, srcSpec.format), resolution(srcSpec.resolution), numMips(srcSpec.numMips)
            {

            }

            Spec(u32 inResolution, u32 inNumMips, GfxTexture::Format inFormat)
                : GfxTexture::Spec(TEX_CUBE, inFormat), resolution(inResolution), numMips(inNumMips)
            {

            }

            bool operator==(const Spec& rhs) const
            {
                return (rhs.resolution == resolution) && (rhs.numMips == numMips) && (rhs.format == format) && (rhs.type == type);
            }

            u32 resolution;
            u32 numMips = 1;
        };

        static TextureCube* create(const Spec& inSpec, const SamplerCube& inSampler = SamplerCube())
        {
            auto outTexture = new TextureCube(inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        Spec getSpec() const
        {
            return Spec(resolution, numMips, format);
        }

        virtual bool operator==(const GfxTexture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const TextureCube*>(&rhs))
            {
                return ptr->getSpec() == getSpec();
            }
            return false;
        }

        SamplerCube sampler;
        u32 resolution = 0;
        u32 numMips = 1;

    protected:
        TextureCube(const Spec& inSpec, const SamplerCube& inSampler = SamplerCube());
        // TextureCube(const char* name, const Spec& inSpec, const SamplerCube& inSampler = SamplerCube());

        virtual void init() override;
    };
}

// custom hash function for texture specs
namespace std
{
    template <>
    struct hash<Cyan::GfxTexture2D::Spec>
    {
        std::size_t operator()(const Cyan::GfxTexture2D::Spec& spec) const
        {
            std::string key = std::to_string(spec.width) + 'x' + std::to_string(spec.height);
            switch (spec.format)
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
                assert(0);
            }
            key += "_" + std::to_string(spec.numMips);
            return std::hash<std::string>()(key);
        }
    };

    template <>
    struct hash<Cyan::GfxDepthTexture2D::Spec>
    {
        std::size_t operator()(const Cyan::GfxDepthTexture2D::Spec& spec) const
        {
            std::string key = std::to_string(spec.width) + 'x' + std::to_string(spec.height);
            switch (spec.format)
            {
            case PF_D24S8:
                key += "_D24S8";
                break;
            default:
                assert(0);
            }
            key += "_" + std::to_string(spec.numMips);
            return std::hash<std::string>()(key);
        }
    };
}