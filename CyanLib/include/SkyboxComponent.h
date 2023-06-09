#pragma once

#include "Common.h"
#include "AssetManager.h"
#include "SceneComponent.h"

namespace Cyan
{
    class Texture2D;
    class Skybox;

    class SkyboxComponent : public SceneComponent
    {
    public:
        SkyboxComponent(const char* name, const Transform& local);
        ~SkyboxComponent();

        /* SceneComponent Interface */
        virtual void setOwner(Entity* owner) override;

        static constexpr const char* defaultHDRIPath = ASSET_PATH "cubemaps/neutral_sky.hdr";

        u32 m_cubemapResolution = 1024;
        std::shared_ptr<Texture2D> m_HDRI = nullptr;
        std::unique_ptr<Skybox> m_skybox = nullptr;
    };
}
