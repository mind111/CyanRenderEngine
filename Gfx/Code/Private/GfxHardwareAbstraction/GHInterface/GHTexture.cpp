#include "GHTexture.h"
#include "GfxHardwareContext.h"

namespace Cyan
{
    std::unique_ptr<GHTexture2D> GHTexture2D::create(const Desc& desc)
    {
        return std::move(GfxHardwareContext::get()->createTexture2D(desc));
    }

    GHTexture2D::GHTexture2D(const Desc& desc)
        : m_desc(desc), m_defaultSampler()
    {
        assert(desc.width > 0 && desc.height > 0 && desc.numMips >= 1);
    }

    PixelFormat translateDepthFormatToPixelFormat(const DepthFormat& df)
    {
        switch (df)
        {
        case DepthFormat::kD24S8: return PixelFormat::kD24S8;
        case DepthFormat::k32F: return PixelFormat::kR32F;
        default: assert(0); return PixelFormat::kCount;
        }
    }

    std::unique_ptr<GHDepthTexture> GHDepthTexture::create(const Desc& desc)
    {
        return std::move(GfxHardwareContext::get()->createDepthTexture(desc));
    }

    GHDepthTexture::GHDepthTexture(const Desc& desc)
        : GHTexture2D(GHTexture2D::Desc{ desc.width, desc.height, desc.numMips, translateDepthFormatToPixelFormat(desc.df), desc.data })
    {
        assert(desc.width > 0 && desc.height > 0 && desc.numMips >= 1);
    }
}
