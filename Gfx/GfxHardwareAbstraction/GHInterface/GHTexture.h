#pragma once

#include "Core.h"

namespace Cyan
{
    class GHSampler2D
    {
    public:
        friend class GHTexture2D;
        friend class GLTexture;
        friend class GLTexture2D;

        enum class AddressingMode
        {
            Wrap = 0,
            Clamp,
            Invalid
        };

        enum class FilteringMode
        {
            Point,
            PointMipmapPoint,
            Bilinear,
            BilinearMipmapPoint,
            Trilinear,
        };

        virtual void setAddressingModeX(const AddressingMode& am) { m_addressingX = am; }
        virtual void setAddressingModeY(const AddressingMode& am) { m_addressingY = am; }
        virtual void setFilteringModeMin(const FilteringMode& fm) { m_filteringMin = fm; }
        virtual void setFilteringModeMag(const FilteringMode& fm) { m_filteringMag = fm; }

    protected:
        GHSampler2D() = default;

        AddressingMode m_addressingX = AddressingMode::Wrap;
        AddressingMode m_addressingY = AddressingMode::Wrap;
        FilteringMode m_filteringMin = FilteringMode::Point;
        FilteringMode m_filteringMag = FilteringMode::Point;
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

    enum class DepthFormat
    {
        kD24S8 = 0,
        k32F,
        kCount
    };

    class GHTexture
    {
    public:
        virtual ~GHTexture() { }

        virtual void init() = 0;
        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    class GHTexture2D : public GHTexture
    {
    public:
        struct Desc
        {
            u32 width = 0;
            u32 height = 0;
            u32 numMips = 1;
            PixelFormat pf = PixelFormat::kCount;
            void* data = nullptr;
        };

        virtual ~GHTexture2D() { }

        virtual void getMipSize(i32& outWidth, i32& outHeight, i32 mipLevel) = 0;

        const Desc& getDesc() { return m_desc; }
    protected:
        GHTexture2D(const Desc& desc);

        Desc m_desc;
        GHSampler2D m_defaultSampler;
    };

    class GHDepthTexture : public GHTexture2D
    {
    public:
        struct Desc
        {
            u32 width = 0;
            u32 height = 0;
            u32 numMips = 1;
            DepthFormat df = DepthFormat::kD24S8;
            void* data = nullptr;
        };

        virtual ~GHDepthTexture() { }
    protected:
        GHDepthTexture(const Desc& desc);
    };

    class GHTextureCube : public GHTexture
    {
    public:
        virtual ~GHTextureCube() { }
    };
}
