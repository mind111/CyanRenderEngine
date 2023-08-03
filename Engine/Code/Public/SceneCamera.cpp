#include "SceneCamera.h"
#include "Transform.h"

namespace Cyan
{
    static constexpr glm::vec2 defaultResolution = glm::uvec2(1920, 1080);

    SceneCamera::SceneCamera(const Transform& transform)
        : SceneCamera(defaultResolution, transform)
    { 
    }

    SceneCamera::SceneCamera(const glm::uvec2& renderResolution, const Transform& transform)
        : m_cameraViewInfo(transform)
        , bPower(true)
        , m_numRenderedFrames(0u)
        , m_elapsedTime(0.f)
        , m_deltaTime(0.f)
        , m_resolution(renderResolution)
        , m_renderMode(RenderMode::kSceneDepth)
    {

    }

    SceneCamera::SceneCamera()
        : m_cameraViewInfo()
        , bPower(true)
        , m_numRenderedFrames(0u)
        , m_elapsedTime(0.f)
        , m_deltaTime(0.f)
        , m_resolution(defaultResolution)
        , m_renderMode(RenderMode::kSceneDepth)
    {

    }

    SceneCamera::~SceneCamera() { }

#if 0
    void SceneCamera::onRenderStart()
    {
        updateSceneViewInfo();
    }

    void SceneCamera::render(IScene* scene)
    {
        if (bPower)
        {
            onRenderStart();
            if (scene != nullptr)
            {
                m_sceneRenderer->render(m_sceneRender.get(), scene, m_sceneViewInfo);
            }
            onRenderFinish();
        }
    }
#endif
}
