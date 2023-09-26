#pragma once

#include "Core.h"
#include "Gfx.h"

namespace Cyan
{
    enum class SamplerAddressingMode
    {
        Wrap = 0,
        Clamp,
        Invalid
    };

    enum class SamplerFilteringMode
    {
        Point,
        PointMipmapPoint,
        Bilinear,
        BilinearMipmapPoint,
        Trilinear,
    };

    class GFX_API GHSampler2D
    {
    public:
        friend class GHTexture2D;
        friend class GLTexture;
        friend class GLTexture2D;

        GHSampler2D() = default;
        GHSampler2D(const SamplerAddressingMode& addrX, const SamplerAddressingMode& addrY, const SamplerFilteringMode& min, const SamplerFilteringMode& mag);
        ~GHSampler2D() = default;

        virtual void setAddressingModeX(const SamplerAddressingMode& am) { m_addressingX = am; }
        virtual void setAddressingModeY(const SamplerAddressingMode& am) { m_addressingY = am; }
        virtual void setFilteringModeMin(const SamplerFilteringMode& fm) { m_filteringMin = fm; }
        virtual void setFilteringModeMag(const SamplerFilteringMode& fm) { m_filteringMag = fm; }

        SamplerAddressingMode getAddressingModeX() const { return m_addressingX; }
        SamplerAddressingMode getAddressingModeY() const { return m_addressingY; }
        SamplerFilteringMode getFilteringModeMin() const { return m_filteringMin; }
        SamplerFilteringMode getFilteringModeMag() const { return m_filteringMag; }

    protected:
        SamplerAddressingMode m_addressingX = SamplerAddressingMode::Wrap;
        SamplerAddressingMode m_addressingY = SamplerAddressingMode::Wrap;
        SamplerFilteringMode m_filteringMin = SamplerFilteringMode::Point;
        SamplerFilteringMode m_filteringMag = SamplerFilteringMode::Point;
    };

#define SAMPLER2D_FM_POINT Cyan::SamplerFilteringMode::Point
#define SAMPLER2D_FM_BILINEAR Cyan::SamplerFilteringMode::Bilinear
#define SAMPLER2D_FM_TRILINEAR Cyan::SamplerFilteringMode::Trilinear
#define SAMPLER2D_AM_WRAP Cyan::SamplerAddressingMode::Wrap
#define SAMPLER2D_AM_CLAMP Cyan::SamplerAddressingMode::Clamp

    class GFX_API GHSamplerCube
    {
    public:
        GHSamplerCube() = default;
        ~GHSamplerCube() = default;

        virtual void setAddressingModeX(const SamplerAddressingMode& am) { m_addressingX = am; }
        virtual void setAddressingModeY(const SamplerAddressingMode& am) { m_addressingY = am; }
        virtual void setFilteringModeMin(const SamplerFilteringMode& fm) { m_filteringMin = fm; }
        virtual void setFilteringModeMag(const SamplerFilteringMode& fm) { m_filteringMag = fm; }

        SamplerAddressingMode getAddressingModeX() const { return m_addressingX; }
        SamplerAddressingMode getAddressingModeY() const { return m_addressingY; }
        SamplerFilteringMode getFilteringModeMin() const { return m_filteringMin; }
        SamplerFilteringMode getFilteringModeMag() const { return m_filteringMag; }

    protected:
        SamplerAddressingMode m_addressingX = SamplerAddressingMode::Clamp;
        SamplerAddressingMode m_addressingY = SamplerAddressingMode::Clamp;
        SamplerFilteringMode m_filteringMin = SamplerFilteringMode::Point;
        SamplerFilteringMode m_filteringMag = SamplerFilteringMode::Point;
    };

    enum class PixelFormat
    {
        kR8 = 0,
        kR16F,
        kR32F,
        kD24S8,
        kRGB8,
        kRGBA8,
        kRGB16F,
        kRGBA16F,
        kRGB32F,
        kRGBA32F,
        kCount
    };

#define PF_R8 Cyan::PixelFormat::kR8
#define PF_R16F Cyan::PixelFormat::kR16F
#define PF_R32F Cyan::PixelFormat::kR32F
#define PF_D24S8 Cyan::PixelFormat::kD24S8
#define PF_RGB8 Cyan::PixelFormat::kRGB8
#define PF_RGBA8 Cyan::PixelFormat::kRGBA8
#define PF_RGB16F Cyan::PixelFormat::kRGB16F
#define PF_RGBA16F Cyan::PixelFormat::kRGBA16F
#define PF_RGB32F Cyan::PixelFormat::kRGB32F
#define PF_RGBA32F Cyan::PixelFormat::kRGBA32F

    enum class DepthFormat
    {
        kD24S8 = 0,
        k32F,
        kCount
    };

    class GFX_API GHTexture
    {
    public:
        virtual ~GHTexture() { }

        virtual void init() = 0;
        virtual void bind() = 0;
        virtual void unbind() = 0;
        virtual void* getGHO() = 0;
    };

    class GFX_API GHTexture2D : public GHTexture
    {
    public:
        struct Desc
        {
            static Desc create(u32 inWidth, u32 inHeight, u32 inNumMips, const PixelFormat& inPf, void* inData = nullptr)
            {
                Desc desc = { };
                desc.width = inWidth;
                desc.height = inHeight;
                desc.numMips = inNumMips;
                desc.pf = inPf;
                desc.data = inData;
                return desc;
            }

            bool operator==(const Desc& rhs) const
            {
                bool isEqual = true;
                isEqual &= (width == rhs.width);
                isEqual &= (height == rhs.height);
                isEqual &= (numMips == rhs.numMips);
                isEqual &= (pf == rhs.pf);
                return isEqual;
            }

            u32 width = 0;
            u32 height = 0;
            u32 numMips = 1;
            PixelFormat pf = PixelFormat::kCount;
            void* data = nullptr;
        };

        static std::unique_ptr<GHTexture2D> create(const Desc& desc, const GHSampler2D& sampler2D = GHSampler2D());
        virtual ~GHTexture2D() { }

        virtual void* getGHO() override { return nullptr; }
        virtual void getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel) = 0;

        const Desc& getDesc() { return m_desc; }
    protected:
        GHTexture2D(const Desc& desc, const GHSampler2D& sampler2D = GHSampler2D());

        Desc m_desc;
        GHSampler2D m_defaultSampler;
    };

    class GFX_API GHDepthTexture : public GHTexture2D
    {
    public:
        struct Desc
        {
            static Desc create(u32 inWidth, u32 inHeight, u32 inNumMips, void* inData = nullptr) 
            {
                Desc desc = { };
                desc.width = inWidth;
                desc.height = inHeight;
                desc.numMips = inNumMips;
                desc.df = DepthFormat::kD24S8;
                desc.data = inData;
                return desc;
            }

            u32 width = 0;
            u32 height = 0;
            u32 numMips = 1;
            DepthFormat df = DepthFormat::kD24S8;
            void* data = nullptr;
        };

        static std::unique_ptr<GHDepthTexture> create(const Desc& desc);

        virtual ~GHDepthTexture() { }
    protected:
        GHDepthTexture(const Desc& desc);
    };

    class GHTextureCube : public GHTexture
    {
    public:
        struct Desc
        {
            static Desc create(u32 inResolution, u32 inNumMips, void* inData = nullptr) 
            {
                Desc desc = { };
                desc.resolution = inResolution;
                desc.numMips = inNumMips;
                desc.pf = PixelFormat::kRGB16F;
                desc.data = inData;
                return desc;
            }

            u32 resolution = 0;
            u32 numMips = 1;
            PixelFormat pf = PixelFormat::kRGB16F;
            void* data = nullptr;
        };

        static std::unique_ptr<GHTextureCube> create(const GHTextureCube::Desc& desc, const GHSamplerCube& samplerCube);
        virtual ~GHTextureCube() { }

        virtual void* getGHO() override { return nullptr; }
        virtual void getMipSize(i32& outSize, i32 mipLevel) = 0;
        const Desc& getDesc() { return m_desc; }
    protected:
        GHTextureCube(const GHTextureCube::Desc& desc, const GHSamplerCube& sampler);

        Desc m_desc;
        GHSamplerCube m_defaultSampler;
    };
}

// custom hash function for texture specs
namespace std
{
    template <>
    struct hash<Cyan::GHTexture2D::Desc>
    {
        std::size_t operator()(const Cyan::GHTexture2D::Desc& desc) const
        {
            std::string key = std::to_string(desc.width) + 'x' + std::to_string(desc.height);
            switch (desc.pf)
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
            key += "_" + std::to_string(desc.numMips);
            return std::hash<std::string>()(key);
        }
    };
}
