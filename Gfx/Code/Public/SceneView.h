#pragma once

#include "Gfx.h"
#include "Core.h"
#include "MathLibrary.h"
#include "GHTexture.h"
#include "CameraViewInfo.h"

namespace Cyan
{
    class SceneRender;

    class GFX_API SceneView
    {
    public:
        friend class Engine;
        friend class SceneRenderer;
        friend class GfxModule;

        // direct mirror of SceneCamera::RenderMode
        enum class ViewMode
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

        struct ViewSettings
        {

        };

        struct State
        {
            glm::uvec2 resolution;
            f32 aspectRatio;
            glm::mat4 viewMatrix;
            glm::mat4 prevFrameViewMatrix;
            glm::mat4 projectionMatrix;
            glm::mat4 prevFrameProjectionMatrix;
            glm::vec3 cameraPosition;
            glm::vec3 prevFrameCameraPosition;
            glm::vec3 cameraLookAt;
            glm::vec3 cameraRight;
            glm::vec3 cameraForward;
            glm::vec3 cameraUp;
            i32 frameCount;
            f32 elapsedTime;
            f32 deltaTime;
        };

        SceneView(const glm::uvec2& renderResolution);
        ~SceneView();

        GHTexture2D* getOutput();

    private:
        std::unique_ptr<SceneRender> m_render = nullptr;
        State m_state;
        CameraViewInfo m_cameraInfo;
        ViewMode m_viewMode = ViewMode::kSceneColor;
    };
}

