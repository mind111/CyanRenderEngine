#include "Camera.h"

namespace Cyan
{
    Camera::Camera()
    {
        m_position = glm::vec3(0.f, 1.f, -2.f); 
        m_lookAt = glm::vec3(0.f, 0.f, -4.f);
        m_worldUp = glm::vec3(0.f, 1.f, 0.f);
        m_renderResolution = glm::vec2(1024, 1024),
        m_viewMode = VM_RESOLVED_SCENE_COLOR;
    }

    Camera::Camera(const glm::vec3& inPosition, const glm::vec3& inLookAt, const glm::vec3& inWorldUp, const glm::vec2& renderResolution, const ViewMode& viewMode)
        : m_position(inPosition), m_lookAt(inLookAt), m_worldUp(inWorldUp), m_renderResolution(renderResolution), m_viewMode(viewMode)
    {
        // todo: register with the IO system
    }

    void Camera::setSceneRender(SceneRender* sceneRender)
    {
        assert(m_sceneRender == nullptr);
        m_sceneRender = sceneRender;
    }
}
