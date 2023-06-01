#include "Camera.h"

namespace Cyan
{
    Camera::Camera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, const glm::vec2& renderResolution, const ViewMode& viewMode)
        : m_position(inPosition), m_lookAt(inLookAt), m_worldUp(inWorldUp), m_renderResolution(renderResolution), m_viewMode(viewMode)
    { 
    }

    void Camera::setSceneRender(SceneRender* sceneRender)
    {
        assert(m_sceneRender == nullptr);
        m_sceneRender = sceneRender;
    }
}
