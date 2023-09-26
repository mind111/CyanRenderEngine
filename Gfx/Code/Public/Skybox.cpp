#include "Skybox.h"
#include "MathLibrary.h"
#include "Shader.h"
#include "RenderingUtils.h"
#include "CameraViewInfo.h"

namespace Cyan
{
    Skybox::Skybox(u32 resolution)
    {
        assert(isPowerOf2(resolution));
        u32 numMips = glm::log2(resolution) + 1;
        const auto& desc = GHTextureCube::Desc::create(resolution, numMips, nullptr);
        GHSamplerCube samplerCube{ };
        samplerCube.setFilteringModeMin(SamplerFilteringMode::Trilinear);
        samplerCube.setFilteringModeMag(SamplerFilteringMode::Bilinear);
        m_cubemap = std::move(GHTextureCube::create(desc, samplerCube));
    }

    Skybox::~Skybox()
    {

    }

    void Skybox::buildFromHDRI(GHTexture2D* HDRITexture)
    {
        GPU_DEBUG_SCOPE(marker, "Build Skybox")

        RenderingUtils::buildCubemapFromHDRI(m_cubemap.get(), HDRITexture);
    }
}