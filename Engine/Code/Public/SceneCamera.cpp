#include "SceneCamera.h"
#include "GfxInterface.h"

namespace Cyan
{
    static constexpr glm::vec2 defaultResolution = glm::uvec2(1920, 1080);

    SceneCamera::SceneCamera()
        : SceneCamera(defaultResolution)
    { }

    SceneCamera::SceneCamera(const glm::uvec2& renderResolution)
        : m_resolution(renderResolution)
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
