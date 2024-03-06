#include "GHTexture.h"
#include "GfxHardwareContext.h"
#include "GfxModule.h"

namespace Cyan
{
    std::unique_ptr<GHTexture2D> GHTexture2D::create(const Desc& desc, const GHSampler2D& sampler2D)
    {
        std::unique_ptr<GHTexture2D> outTex2D = std::move(GfxHardwareContext::get()->createTexture2D(desc, sampler2D));
        outTex2D->init();
        return std::move(outTex2D);
    }

    GHTexture2D::GHTexture2D(const Desc& desc, const GHSampler2D& sampler2D)
        : m_desc(desc), m_defaultSampler(sampler2D)
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
        std::unique_ptr<GHDepthTexture> outDepthTex = std::move(GfxHardwareContext::get()->createDepthTexture(desc));
        outDepthTex->init();
        return std::move(outDepthTex);
    }

    GHDepthTexture::GHDepthTexture(const Desc& desc)
        : GHTexture2D(GHTexture2D::Desc{ desc.width, desc.height, desc.numMips, translateDepthFormatToPixelFormat(desc.df), desc.data })
    {
        assert(desc.width > 0 && desc.height > 0 && desc.numMips >= 1);
    }

    std::unique_ptr<GHTextureCube> GHTextureCube::create(const Desc& desc, const GHSamplerCube& samplerCube)
    {
        std::unique_ptr<GHTextureCube> outTexCube = std::move(GfxHardwareContext::get()->createTextureCube(desc, samplerCube));
        outTexCube->init();
        return std::move(outTexCube);
    }

    GHTextureCube::GHTextureCube(const GHTextureCube::Desc& desc, const GHSamplerCube& sampler)
        : m_desc(desc)
        , m_defaultSampler(sampler)
    {
        assert(desc.resolution > 0 && desc.numMips >= 1);
    }

    GHSampler2D::GHSampler2D(const SamplerAddressingMode& addrX, const SamplerAddressingMode& addrY, const Sampler2DFilteringMode& min, const Sampler2DFilteringMode& mag)
    {
        setAddressingModeX(addrX);
        setAddressingModeY(addrY);
        setFilteringModeMin(min);
        setFilteringModeMag(mag);
    }

    GHSampler3D::GHSampler3D(const SamplerAddressingMode& addrX, const SamplerAddressingMode& addrY, const SamplerAddressingMode& addrZ, const Sampler3DFilteringMode& min, const Sampler3DFilteringMode& mag)
    {
        setAddressingModeX(addrX);
        setAddressingModeY(addrY);
        setAddressingModeZ(addrZ);
        setFilteringModeMin(min);
        setFilteringModeMag(mag);
    }

    const GHSampler3D GHSampler3D::create(const Sampler3DFilteringMode& minFilter /*= SAMPLER3D_FM_POINT*/, const Sampler3DFilteringMode& magFilter /*= SAMPLER3D_FM_POINT*/, const SamplerAddressingMode& x /*= SAMPLER3D_AM_CLAMP*/, const SamplerAddressingMode& y /*= SAMPLER3D_AM_CLAMP*/, const SamplerAddressingMode& z /*= SAMPLER3D_AM_CLAMP*/)
    {
        GHSampler3D outSampler3D;
        outSampler3D.setFilteringModeMin(minFilter);
        outSampler3D.setFilteringModeMin(magFilter);
        outSampler3D.setAddressingModeX(x);
        outSampler3D.setAddressingModeY(y);
        outSampler3D.setAddressingModeZ(z);
        return outSampler3D;
    }

    std::unique_ptr<Cyan::GHTexture3D> GHTexture3D::create(const GHTexture3D::Desc& desc, const GHSampler3D& sampler3D)
    {
        std::unique_ptr<GHTexture3D> outTex3D = std::move(GfxHardwareContext::get()->createTexture3D(desc, sampler3D));
        outTex3D->init();
        return std::move(outTex3D);
    }

    GHTexture3D::GHTexture3D(const GHTexture3D::Desc& desc, const GHSampler3D& sampler)
        : m_desc(desc)
        , m_defaultSampler(sampler)
    {
        assert(desc.width > 0 && desc.height > 0 && desc.depth > 0 && desc.numMips >= 1);
    }

}
