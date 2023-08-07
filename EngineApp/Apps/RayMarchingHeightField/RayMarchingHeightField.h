#pragma once

#include "App.h"

namespace Cyan
{
    class GHTexture2D;

    class RayMarchingHeightFieldApp : public App
    {
    public:
        RayMarchingHeightFieldApp(i32 windowWidth, i32 windowHeight);
        ~RayMarchingHeightFieldApp();

        virtual void update(World* world) override;
    protected:
        virtual void customInitialize(World* world) override;
    private:
        std::unique_ptr<GHTexture2D> m_heightFieldTexture = nullptr;
        std::unique_ptr<GHTexture2D> m_rayMarchingOutTexture = nullptr;
    };
}
