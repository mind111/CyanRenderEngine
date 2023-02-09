#pragma once

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
#define TEX_2D Cyan::Texture::Type::kTex2D
#define TEX_2D_ARRAY Cyan::Texture::Type::kTex2DArray
#define TEX_DEPTH Cyan::Texture::Type::kDepth2D
#define TEX_3D Cyan::Texture::Type::kTex3D
#define TEX_CUBE Cyan::Texture::Type::kTexCube

// WM stands for "wrap mode"
#define WM_CLAMP Cyan::Sampler::Addressing::kClampToEdge
#define WM_WRAP Cyan::Sampler::Addressing::kWrap

// FM stands for "filter mode"
#define FM_POINT Cyan::Sampler::Filtering::kPoint
#define FM_MIPMAP_POINT Cyan::Sampler::Filtering::kMipmapPoint
#define FM_BILINEAR Cyan::Sampler::Filtering::kBilinear
#define FM_TRILINEAR Cyan::Sampler::Filtering::kTrilinear

// PF stands for "pixel format" 
#define PF_R8 Cyan::Texture::Format::kR8
#define PF_RGB8 Cyan::Texture::Format::kRGB8
#define PF_RGBA8 Cyan::Texture::Format::kRGBA8
#define PF_R16F Cyan::Texture::Format::kR16F
#define PF_RGB16F Cyan::Texture::Format::kRGB16F
#define PF_RGBA16F Cyan::Texture::Format::kRGBA16F
#define PF_D24S8 Cyan::Texture::Format::kD24S8
#define PF_RGB32F Cyan::Texture::Format::kRGB32F
#define PF_RGBA32F Cyan::Texture::Format::kRGBA32F

namespace Cyan
{
    struct Texture : public GpuResource
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

        virtual bool operator==(const Texture& rhs) const = 0;

        Texture(const Format& inFormat);
        Texture(const char* inName, const Format& inFormat);
        virtual ~Texture() { }

        virtual void init() = 0;

        static u32 count;
        std::string name;
        Format format;
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
    struct Texture2D;
    struct Texture2DArray;
    struct TextureCube;

    struct Sampler2D : public Sampler
    {
        void init(Texture2D* texture);

        Addressing wrapS = Addressing::kClampToEdge;
        Addressing wrapT = Addressing::kClampToEdge;
        Filtering minFilter = Filtering::kPoint;
        Filtering magFilter = Filtering::kPoint;
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

    struct Texture2D : public Texture
    {
        struct Spec : public Texture::Spec
        {
            Spec()
                : Texture::Spec(TEX_2D)
            {
            }

            Spec(const Type& inType)
                : Texture::Spec(inType)
            {
            }

            Spec(const Type& inType, const Format& inFormat)
                : Texture::Spec(inType, inFormat)
            {
            }

            Spec(const Spec& srcSpec)
                : Texture::Spec(TEX_2D, srcSpec.format), width(srcSpec.width), height(srcSpec.height), numMips(srcSpec.numMips)
            {
            }

            Spec(u32 inWidth, u32 inHeight, u32 inNumMips, Texture::Format inFormat)
                : Texture::Spec(TEX_2D, inFormat), width(inWidth), height(inHeight), numMips(inNumMips)
            {
            }

            Spec(const Type& inType, u32 inWidth, u32 inHeight, u32 inNumMips, Texture::Format inFormat)
                : Texture::Spec(inType, inFormat), width(inWidth), height(inHeight), numMips(inNumMips)
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

        Texture2D(const Image& inImage, bool bGenerateMipmap, const Sampler2D& inSampler = {});
        Texture2D(const char* inName, const Image& inImage, bool bGenerateMipmap, const Sampler2D& inSampler = {});
        Texture2D(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        Texture2D(const char* inName, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());

        Spec getSpec() const
        {
            return Spec(width, height, numMips, format);
        }

        virtual bool operator==(const Texture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const Texture2D*>(&rhs))
            {
                return ptr->name == name && ptr->getSpec() == getSpec();
            }
            return false;
        }

        virtual void init() override;

        Image* srcImage = nullptr;
        Sampler2D sampler;
        u32 width = 0;
        u32 height = 0;
        u32 numMips = 1;
    };

    struct Texture2DBindless : public Texture2D
    {
        Texture2DBindless(const Image& inImage, bool bGenerateMipmap, const Sampler2D& inSampler = {});
        Texture2DBindless(const char* inName, const Image& inImage, bool bGenerateMipmap, const Sampler2D& inSampler = {});
        Texture2DBindless(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        Texture2DBindless(const char* inName, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());

        virtual void init() override;

#if BINDLESS_TEXTURE
        u64 glHandle;
#endif
    };

    struct DepthTexture2D : public Texture2D
    {
        struct Spec : public Texture2D::Spec 
        {
            Spec()
                : Texture2D::Spec(TEX_DEPTH, PF_D24S8)
            {
            }

            Spec(const Spec& srcSpec)
                : Texture2D::Spec(TEX_DEPTH, srcSpec.width, srcSpec.height, srcSpec.numMips, PF_D24S8)
            {
            }

            Spec(u32 inWidth, u32 inHeight, u32 inNumMips)
                : Texture2D::Spec(TEX_DEPTH, inWidth, inHeight, numMips, PF_D24S8)
            {
            }

            bool operator==(const Spec& rhs) const
            {
                return (rhs.width == width) && (rhs.height == height) && (rhs.numMips == numMips) && (rhs.format == format) && (rhs.type == type);
            }
        };

        DepthTexture2D(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        DepthTexture2D(const char* inName, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());

        Spec getSpec() const
        {
            return Spec(width, height, numMips);
        }

        virtual bool operator==(const Texture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const DepthTexture2D*>(&rhs))
            {
                return ptr->name == name && ptr->getSpec() == getSpec();
            }
            return false;
        }

        virtual void init() override;
    };

    struct DepthTexture2DBindless : public DepthTexture2D
    {
        DepthTexture2DBindless(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        DepthTexture2DBindless(const char* inName, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());

        virtual void init() override;

#if BINDLESS_TEXTURE
        u64 glHandle;
#endif
    };

    struct Texture2DArray : public Texture2D
    {
        struct Spec : public Texture2D::Spec
        {
            Spec()
                : Texture2D::Spec(TEX_2D_ARRAY)
            {
            }

            Spec(const Spec& srcSpec)
                : Texture2D::Spec(TEX_2D_ARRAY, srcSpec.width, srcSpec.height, srcSpec.numMips, srcSpec.format), numLayers(srcSpec.numLayers)
            {
            }

            Spec(u32 inWidth, u32 inHeight, u32 inNumMips, u32 inNumLayers, Texture::Format inFormat)
                : Texture2D::Spec(TEX_2D_ARRAY, inWidth, inHeight, inNumMips, inFormat), numLayers(inNumLayers)
            {
            }

            bool operator==(const Spec& rhs) const
            {
                return (rhs.width == width) && (rhs.height == height) && (rhs.numLayers == numLayers) && (rhs.numMips == numMips) && (rhs.format == format) && (rhs.type == type);
            }

            u32 numLayers = 1;
        };

        Texture2DArray(const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());
        Texture2DArray(const char* inName, const Spec& inSpec, const Sampler2D& inSampler = Sampler2D());

        Spec getSpec() const
        {
            return Spec(width, height, numMips, numLayers, format);
        }

        virtual bool operator==(const Texture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const Texture2DArray*>(&rhs))
            {
                return ptr->name == name && ptr->getSpec() == getSpec();
            }
            return false;
        }

        virtual void init() override;

        u32 numLayers = 1;
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

    struct TextureCube : public Texture
    {
        struct Spec : public Texture::Spec
        {
            Spec()
                : Texture::Spec(TEX_CUBE)
            {

            }

            Spec(const Spec& srcSpec)
                : Texture::Spec(TEX_CUBE, srcSpec.format), resolution(srcSpec.resolution), numMips(srcSpec.numMips)
            {

            }

            Spec(u32 inResolution, u32 inNumMips, Texture::Format inFormat)
                : Texture::Spec(TEX_CUBE, inFormat), resolution(inResolution), numMips(inNumMips)
            {

            }

            bool operator==(const Spec& rhs) const
            {
                return (rhs.resolution == resolution) && (rhs.numMips == numMips) && (rhs.format == format) && (rhs.type == type);
            }

            u32 resolution;
            u32 numMips = 1;
        };

        TextureCube(const Spec& inSpec, const SamplerCube& inSampler = SamplerCube());
        TextureCube(const char* name, const Spec& inSpec, const SamplerCube& inSampler = SamplerCube());

        Spec getSpec() const
        {
            return Spec(resolution, numMips, format);
        }

        virtual bool operator==(const Texture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const TextureCube*>(&rhs))
            {
                return ptr->name == name && ptr->getSpec() == getSpec();
            }
            return false;
        }

        virtual void init() override;

        SamplerCube sampler;
        u32 resolution = 0;
        u32 numMips = 1;
    };
}

// custom hash function for ITexture::Spec
namespace std
{
    template <>
    struct hash<Cyan::Texture2D::Spec>
    {
        std::size_t operator()(const Cyan::Texture2D::Spec& spec) const
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
                break;
            }
            return std::hash<std::string>()(key);
        }
    };
}