#include "SceneView.h"
#include "SceneRender.h"
#include "CameraViewInfo.h"

namespace Cyan
{
    SceneView::SceneView(const glm::uvec2& renderResolution)
    {
        m_render = std::make_unique<SceneRender>(renderResolution);

        m_state.resolution = renderResolution;
        m_state.aspectRatio = 1.f;
        m_state.viewMatrix = glm::mat4(1.f);
        m_state.prevFrameViewMatrix = glm::mat4(1.f);
        m_state.projectionMatrix = glm::mat4(1.f);
        m_state.prevFrameProjectionMatrix = glm::mat4(1.f);
        m_state.cameraPosition = glm::vec3(0.f);
        m_state.prevFrameCameraPosition = glm::vec3(0.f);
        m_state.cameraLookAt = glm::vec3(0.f);
        m_state.cameraRight = glm::vec3(0.f);
        m_state.cameraForward = glm::vec3(0.f);
        m_state.cameraUp = glm::vec3(0.f, 1.f, 0.f);
        m_state.frameCount = 0;
        m_state.elapsedTime = 0.f;
        m_state.deltaTime = 0.f;

        m_cameraInfo = CameraViewInfo();
    }

    SceneView::~SceneView()
    {
    }

    GHTexture2D* SceneView::getOutput()
    {
        switch (m_viewMode)
        {
        case ViewMode::kSceneColor: return m_render->color();
        case ViewMode::kResolvedSceneColor: return m_render->resolvedColor();
        case ViewMode::kSceneAlbedo: return m_render->albedo();
        case ViewMode::kSceneDepth: return m_render->depth();
        case ViewMode::kSceneNormal: return m_render->normal();
        case ViewMode::kSceneDirectLighting: return m_render->directLighting();
        case ViewMode::kSceneIndirectLighting: return m_render->indirectLighting();
        default:
            assert(0);
            return nullptr;
        }
    }

    void SceneView::addTask(const SceneRenderingStage& stage, SceneViewTask&& task)
    {
        m_taskQueues[(i32)stage].push(std::move(task));
    }

    void SceneView::flushTasks(const SceneRenderingStage& stage)
    {
        auto& taskQueue = m_taskQueues[(i32)stage];
        while (!taskQueue.empty())
        {
            auto& task = taskQueue.front();
            task(*this);
            taskQueue.pop();
        }
    }
}
