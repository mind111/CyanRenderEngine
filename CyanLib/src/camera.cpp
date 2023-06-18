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

        m_worldUp = worldUpVector();

        m_projectionMode = ProjectionMode::kPerspective;
        m_perspective.fov = 90.f;
        m_perspective.aspectRatio = 1.f;
        m_perspective.n = 0.01f;
        m_perspective.f = 100.f;

        m_orthographic.viewSpaceAABB = BoundingBox3D();
    }

    Camera::Camera(const Transform& localTransform)
        : m_worldSpacePosition(localTransform.translation)
        , m_localSpaceRight(localRight)
        , m_localSpaceForward(localForward)
        , m_localSpaceUp(localUp)
        , m_worldSpaceRight(localRight)
        , m_worldSpaceForward(localForward)
        , m_worldSpaceUp(localUp)
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

        m_worldUp = worldUpVector();

        m_projectionMode = ProjectionMode::kPerspective;
        m_perspective.fov = 90.f;
        m_perspective.aspectRatio = 1.f;
        m_perspective.n = 0.01f;
        m_perspective.f = 100.f;

        m_orthographic.viewSpaceAABB = BoundingBox3D();
    }

    Camera::~Camera()
    {

    }
}
