#pragma once

#include "LightProbe.h"
#include "RenderTexture.h"
#include "Entity.h"
#include "AssetManager.h"

namespace Cyan
{
    class SkyboxComponent;

    class Skybox
    {
    public:
        Skybox(SkyboxComponent* skyboxComponent);
        ~Skybox() { }

        SkyboxComponent* m_skyboxComponent = nullptr;
        std::unique_ptr<GfxTextureCube> m_cubemap = nullptr;
    };
}
