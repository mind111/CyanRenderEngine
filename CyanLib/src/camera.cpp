#include "Camera.h"

namespace Cyan
{
    Camera::Camera()
    {
        m_localSpaceRight = glm::vec3(1.f, 0.f, 0.f);
        m_localSpaceForward = glm::vec3(0.f, 0.f, -1.f);
        m_localSpaceUp = glm::vec3(0.f, 1.f, 0.f);

        m_worldSpaceRight = glm::vec3(1.f, 0.f, 0.f);
        m_worldSpaceForward = glm::vec3(0.f, 0.f, -1.f);
        m_worldSpaceUp = glm::vec3(0.f, 1.f, 0.f);
        m_worldSpacePosition = glm::vec3(0.f, 1.f, -2.f); 

        m_worldUp = glm::vec3(0.f, 1.f, 0.f);
        m_renderResolution = glm::vec2(1024, 1024),
        m_viewMode = VM_RESOLVED_SCENE_COLOR;
    }

    Camera::Camera(const Transform& localTransform, const glm::vec3& worldUp, const glm::vec2& renderResolution, const ViewMode& viewMode)
        : m_worldSpacePosition(localTransform.translation)
        , m_worldUp(worldUp)
        , m_localSpaceRight(localRight)
        , m_localSpaceForward(localForward)
        , m_localSpaceUp(localUp)
        , m_worldSpaceRight(localRight)
        , m_worldSpaceForward(localForward)
        , m_worldSpaceUp(localUp)
        , m_renderResolution(renderResolution)
        , m_viewMode(viewMode)
    {
        // default camera pose
        glm::mat4 m = localTransform.toMatrix();
        // transform by localTransform
        m_localSpaceRight = m * glm::vec4(localRight, 0.f);
        m_localSpaceForward = m * glm::vec4(localForward, 0.f);
        m_localSpaceUp = m * glm::vec4(localUp, 0.f);
        m_localSpaceRight = glm::normalize(m_localSpaceRight);
        m_localSpaceForward = glm::normalize(m_localSpaceForward);
        m_localSpaceUp = glm::normalize(m_localSpaceUp);

        m_worldSpaceRight = m_localSpaceRight;
        m_worldSpaceForward = m_localSpaceForward;
        m_worldSpaceUp = m_localSpaceUp;
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
