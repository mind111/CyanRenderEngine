#pragma once

#include "Gfx.h"
#include "Core.h"
#include "MathLibrary.h"
#include "GfxHardwareAbstraction/GHInterface/GHTexture.h"
#include "CameraViewInfo.h"
#include "Shader.h"

namespace Cyan
{
    class SceneRender;

    using SceneViewTask = std::function<void(class SceneView& view)>;

    enum class SceneRenderingStage
    {
        kPreDepthPrepass = 0,
        kPostDepthPrepass,
        kPreGBuffer,
        kPostGBuffer,
        kCount
    };

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
            void setShaderParameters(Pipeline* p) const
            {
                p->setUniform("viewParameters.renderResolution", resolution);
                p->setUniform("viewParameters.aspectRatio", aspectRatio);
                p->setUniform("viewParameters.viewMatrix", viewMatrix);
                p->setUniform("viewParameters.prevFrameViewMatrix", prevFrameViewMatrix);
                p->setUniform("viewParameters.projectionMatrix", projectionMatrix);
                p->setUniform("viewParameters.prevFrameProjectionMatrix", prevFrameProjectionMatrix);
                p->setUniform("viewParameters.cameraPosition", cameraPosition);
                p->setUniform("viewParameters.prevFrameCameraPosition", prevFrameCameraPosition);
                p->setUniform("viewParameters.cameraRight", cameraRight);
                p->setUniform("viewParameters.cameraForward", cameraForward);
                p->setUniform("viewParameters.cameraUp", cameraUp);
                p->setUniform("viewParameters.frameCount", frameCount);
                p->setUniform("viewParameters.elapsedTime", elapsedTime);
                p->setUniform("viewParameters.deltaTime", deltaTime);
            }

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

        SceneRender* getRender() { return m_render.get(); }
        GHTexture2D* getOutput();
        const State& getState() { return m_state; }
        const CameraViewInfo& getCameraInfo() { return m_cameraInfo; }
        void addTask(const SceneRenderingStage& stage, SceneViewTask&& task);
        void flushTasks(const SceneRenderingStage& stage);
    private:
        std::unique_ptr<SceneRender> m_render = nullptr;
        State m_state;
        CameraViewInfo m_cameraInfo;
        ViewMode m_viewMode = ViewMode::kSceneColor;
        std::queue<SceneViewTask> m_taskQueues[(i32)SceneRenderingStage::kCount];
    };
}

