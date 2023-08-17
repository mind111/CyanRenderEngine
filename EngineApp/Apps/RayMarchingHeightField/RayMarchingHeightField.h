#pragma once

#include "App.h"

#define GPU_NOISE 1

namespace Cyan
{
    class GHTexture2D;
    class Texture2D;

    class RayMarchingHeightFieldApp : public App
    {
    public:
        RayMarchingHeightFieldApp(i32 windowWidth, i32 windowHeight);
        ~RayMarchingHeightFieldApp();

        virtual void update(World* world) override;
    protected:
        virtual void customInitialize(World* world) override;
    private:
#if GPU_NOISE
        // this is only used on render thread
        f32 octaveRandomRotationAngles[16];
        std::unique_ptr<GHTexture2D> m_gpuBaseLayerHeightMap = nullptr;
        std::unique_ptr<GHTexture2D> m_gpuBaseLayerNormalMap = nullptr;
        std::unique_ptr<GHTexture2D> m_gpuCompositedHeightMap = nullptr;
        std::unique_ptr<GHTexture2D> m_gpuCompositedNormalMap = nullptr;
        std::unique_ptr<GHTexture2D> m_gpuCloudOpacityMap = nullptr;

        std::unique_ptr<GHTexture2D> m_heightFieldQuadTree = nullptr;
#else
        std::unique_ptr<GHTexture2D> m_cpuHeightMap = nullptr;
        std::unique_ptr<GHTexture2D> m_cpuNormalMap = nullptr;
#endif
        std::unique_ptr<GHTexture2D> m_rayMarchingOutTexture = nullptr;
    };
}
