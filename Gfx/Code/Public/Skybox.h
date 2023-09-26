#pragma once

#include "Core.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"

namespace Cyan
{
    class GFX_API Skybox
    {
    public:
        Skybox(u32 resolution);
        ~Skybox();

        GHTextureCube* getCubemapTexture() { return m_cubemap.get(); }
        void buildFromHDRI(GHTexture2D* HDRITexture);
    private:
        std::unique_ptr<GHTextureCube> m_cubemap = nullptr;
    };
}
