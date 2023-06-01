#pragma once

// #define BINDLESS_TEXTURE

#include <unordered_map>

#include "glm/glm.hpp"

#include "Common.h"
#include "CyanCore.h"

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
#define PF_R32F Cyan::GfxTexture::Format::kR32F
#define PF_RGB16F Cyan::GfxTexture::Format::kRGB16F
#define PF_RGBA16F Cyan::GfxTexture::Format::kRGBA16F
#define PF_D24S8 Cyan::GfxTexture::Format::kD24S8
#define PF_RGB32F Cyan::GfxTexture::Format::kRGB32F
#define PF_RGBA32F Cyan::GfxTexture::Format::kRGBA32F

namespace Cyan
{
    using TextureHandle = u64;

    struct GfxContext;
    struct BindlessTextureHandle;

    /**
     * Encapsulate a gl texture
     */
    struct GfxTexture : public GfxResource
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
            kR16F,
            kR32F,
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
        virtual ~GfxTexture();

        void bind(GfxContext* ctx, u32 inTextureUnit);
        void unbind(GfxContext* ctx);
        bool isBound();
        i32 getBindingUnit();
#ifdef BINDLESS_TEXTURE
        void setBindlessHandle(BindlessTextureHandle* inBindlessHandle);
#endif

        Format format;
    protected:
        GfxTexture(const Format& inFormat);
        virtual void init() = 0;

        i32 textureUnit = -1;
#ifdef BINDLESS_TEXTURE
        std::shared_ptr<BindlessTextureHandle> bindlessHandle = nullptr;
#endif
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
    struct GfxTextureCube;

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
        void init(GfxTextureCube* texture);

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

    struct GfxTextureCube : public GfxTexture
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

        static GfxTextureCube* create(const Spec& inSpec, const SamplerCube& inSampler = SamplerCube())
        {
            auto outTexture = new GfxTextureCube(inSpec, inSampler);
            outTexture->init();
            return outTexture;
        }

        Spec getSpec() const
        {
            return Spec(resolution, numMips, format);
        }

        virtual bool operator==(const GfxTexture& rhs) const override
        {
            if (auto ptr = dynamic_cast<const GfxTextureCube*>(&rhs))
            {
                return ptr->getSpec() == getSpec();
            }
            return false;
        }

        SamplerCube sampler;
        u32 resolution = 0;
        u32 numMips = 1;

    protected:
        GfxTextureCube(const Spec& inSpec, const SamplerCube& inSampler = SamplerCube());

        virtual void init() override;
    };

#ifdef BINDLESS_TEXTURE
    struct BindlessTextureHandle
    {
        ~BindlessTextureHandle()
        {

        }

        static BindlessTextureHandle* create(GfxTexture* inGfxTexture) 
        { 
            std::shared_ptr<BindlessTextureHandle> outBindlessHandle = nullptr;
            auto entry = bindlessHandleMap.find(inGfxTexture->getGpuResource());
            if (entry != bindlessHandleMap.end())
            {
                // if there is already a bindless handle created for "inGfxTexture" then return the cached instance
                outBindlessHandle = entry->second;
            }
            else
            {
                // or else, create a new bindless texture handle for "inGfxTexture"
                outBindlessHandle = std::shared_ptr<BindlessTextureHandle>(new BindlessTextureHandle(inGfxTexture));
                bindlessHandleMap.insert({ inGfxTexture->getGpuResource(), outBindlessHandle });
            }
            return outBindlessHandle.get();
        }

        TextureHandle getHandle()
        {
            return handle;
        }

        bool isResident()
        {
            return bResident;
        }

        void makeResident() 
        { 
            if (!isResident())
            {
                glMakeTextureHandleResidentARB(handle);
                bResident = true;
            }
        }

        void makeNonResident() 
        { 
            if (isResident())
            {
                glMakeTextureHandleNonResidentARB(handle);
                bResident = false;
            }
        }

        void onGfxTextureBeginRelease()
        {
            auto entry = bindlessHandleMap.find(gfxTexture->getGpuResource());
            assert(entry != bindlessHandleMap.end());
            bindlessHandleMap.erase(entry);
            gfxTexture->setBindlessHandle(nullptr);
        }

    protected:
        TextureHandle handle;
        GfxTexture* gfxTexture = nullptr;
        bool bResident = false;
    private:
        BindlessTextureHandle(GfxTexture* inGfxTexture)
            : gfxTexture(inGfxTexture)
        {
            handle = glGetTextureHandleARB(gfxTexture->getGpuResource());
            inGfxTexture->setBindlessHandle(this);
        }

        static std::unordered_map<u32, std::shared_ptr<BindlessTextureHandle>> bindlessHandleMap;
    };
#endif

    struct RenderTarget
    {
        RenderTarget() = default;
        RenderTarget(GfxTexture2D* inTexture, u32 inMip = 0, const glm::vec4& inClearColor = glm::vec4(0.f, 0.f, 0.f, 1.f))
            : texture(inTexture), mip(inMip), clearColor(inClearColor)
        { 
            if (texture)
            {
                width = Max(inTexture->width / pow(2, mip), width);
                height = Max(inTexture->height / pow(2, mip), height);
                assert(width > 0);
                assert(height > 0);
            }
        }

        RenderTarget(GfxTextureCube* inTexture, u32 inLayer, u32 inMip = 0, const glm::vec4& inClearColor = glm::vec4(0.f, 0.f, 0.f, 1.f))
            : texture(inTexture), mip(inMip), layer(inLayer), clearColor(inClearColor)
        { 
            if (texture)
            {
                width = Max(inTexture->resolution / pow(2, mip), width);
                height = Max(inTexture->resolution / pow(2, mip), height);
                assert(width > 0);
                assert(height > 0);
            }
        }

        GfxTexture* texture = nullptr;
        u32 width = 1;
        u32 height = 1;
        u32 mip = 0;
        u32 layer = 0; // this is only used if texture is of type GfxTextureCube
        glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
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
            case PF_R16F:
                key += "_R16F";
                break;
            case PF_R32F:
                key += "_R32F";
                break;
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
