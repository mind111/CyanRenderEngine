#pragma once

#include <memory>

#include "Core.h"
#include "MathLibrary.h"
#include "Engine.h"
#include "CameraViewInfo.h"
#include "GfxInterface.h"

namespace Cyan
{
    class IScene;
    class ISceneRender;
    class ISceneRenderer;

    /**
     * A camera used for scene rendering
     */
    class ENGINE_API SceneCamera
    {
    public:
        friend class Engine;

        enum class RenderMode
        {
            kSceneColor = 0,
            kResolvedSceneColor,
            kSceneAlbedo,
            kSceneDepth,
            kSceneNormal,
            kSceneDirectLighting,
            kSceneIndirectLighting,
            kSceneLightingOnly,
            kSSGIAO,
            kSSGIDiffuse,
            kDebug,
            kWireframe,
            kCount
        };

        struct RenderSettings
        {

        };

        SceneCamera();
        SceneCamera(const glm::uvec2& renderResolution);
        ~SceneCamera();

        const glm::uvec2& getRenderResolution() { return m_resolution; }

        void turnOn() { bPower = true; }
        void turnOff() { bPower = false; }

#if 0
        void onRenderStart();
        void render(IScene* scene);
        void onRenderFinish();
        void updateSceneViewInfo();
#endif

    private:

        CameraViewInfo m_cameraViewInfo;
        bool bPower = true;
        u32 m_numRenderedFrames = 0u;
        f32 m_elapsedTime = 0.f;
        f32 m_deltaTime = 0.f;
        glm::uvec2 m_resolution;
        RenderMode m_renderMode;
    };
}
