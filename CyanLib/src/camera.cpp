#include "Camera.h"

namespace Cyan
{
    Camera::Camera()
    {
        m_position = glm::vec3(0.f, 1.f, -2.f); 
        m_right = glm::vec3(1.f, 0.f, 0.f);
        m_forward = glm::vec3(0.f, 0.f, -1.f);
        m_up = glm::vec3(0.f, 1.f, 0.f);
        m_worldUp = glm::vec3(0.f, 1.f, 0.f);
        m_renderResolution = glm::vec2(1024, 1024),
        m_viewMode = VM_RESOLVED_SCENE_COLOR;
    }

    Camera::Camera(const Transform& localTransform, const glm::vec3& worldUp, const glm::vec2& renderResolution, const ViewMode& viewMode)
        : m_position(localTransform.translation)
        , m_worldUp(worldUp)
        , m_right(localRight)
        , m_forward(localForward)
        , m_up(localUp)
        , m_renderResolution(renderResolution)
        , m_viewMode(viewMode)
    {
        // default camera pose
        glm::mat4 m = localTransform.toMatrix();
        // transform by localTransform
        m_right = m * glm::vec4(localRight, 0.f);
        m_forward = m * glm::vec4(localForward, 0.f);
        m_up = m * glm::vec4(localUp, 0.f);
        m_right = glm::normalize(m_right);
        m_forward = glm::normalize(m_forward);
        m_up = glm::normalize(m_up);
    }

    Camera::~Camera()
    {

    }

    void Camera::setSceneRender(SceneRender* sceneRender)
    {
        assert(m_sceneRender == nullptr);
        m_sceneRender = sceneRender;
    }
}
