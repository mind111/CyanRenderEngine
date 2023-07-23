#include "GHTexture.h"

namespace Cyan
{
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

    GHDepthTexture::GHDepthTexture(const Desc& desc)
        : GHTexture2D(GHTexture2D::Desc{ desc.width, desc.height, desc.numMips, translateDepthFormatToPixelFormat(desc.df), desc.data })
    {
        assert(desc.width > 0 && desc.height > 0 && desc.numMips >= 1);
    }
}
