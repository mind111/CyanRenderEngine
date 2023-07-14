#pragma once

#include <memory>

#include "Core.h"
#include "MathLibrary.h"
#include "GameFramework.h"
#include "CameraViewInfo.h"

namespace Cyan
{
    class IScene;
    class ISceneRender;
    class ISceneRenderer;

    /**
     * A camera used for scene rendering
     */
    class GAMEFRAMEWORK_API Camera
    {
    public:
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

        Camera(const glm::uvec2& renderResolution);
        ~Camera();

        void turnOn() { bPower = true; }
        void turnOff() { bPower = false; }

        void onRenderStart();
        void render(IScene* scene);
        void onRenderFinish();

    private:
        void updateSceneViewInfo();

        CameraViewInfo m_cameraViewInfo;
        SceneViewInfo m_sceneViewInfo;
        std::unique_ptr<ISceneRenderer> m_sceneRenderer = nullptr;
        std::unique_ptr<ISceneRender> m_sceneRender = nullptr;

        bool bPower = true;
        u32 m_numRenderedFrames = 0u;
        f32 m_elapsedTime = 0.f;
        f32 m_deltaTime = 0.f;
        glm::uvec2 m_resolution;
        RenderMode m_renderMode;
    };
}
