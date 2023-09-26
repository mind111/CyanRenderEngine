#include "CameraViewInfo.h"
#include "Transform.h"

namespace Cyan
{
    CameraViewInfo::CameraViewInfo()
    {
        m_localSpaceRight = glm::vec3(1.f, 0.f, 0.f);
        m_localSpaceForward = glm::vec3(0.f, 0.f, -1.f);
        m_localSpaceUp = glm::vec3(0.f, 1.f, 0.f);

        m_worldSpaceRight = glm::vec3(1.f, 0.f, 0.f);
        m_worldSpaceForward = glm::vec3(0.f, 0.f, -1.f);
        m_worldSpaceUp = glm::vec3(0.f, 1.f, 0.f);
        m_worldSpacePosition = glm::vec3(0.f, 0.f, 0.f);

        m_worldUp = worldUpVector();

        m_projectionMode = ProjectionMode::kPerspective;
        m_perspective.fov = 90.f;
        m_perspective.aspectRatio = 1.f;
        /**
         * todo: Run into depth precision issues when using a near plane of .01 (1cm), especially
         * visible in screen space ray traced effects. Need to learn about how to combat these issues
         */
        m_perspective.n = 0.01f;
        // m_perspective.n = 1.0f;
        m_perspective.f = 100.f;

        m_orthographic.viewSpaceAABB = AxisAlignedBoundingBox3D();
    }

    CameraViewInfo::CameraViewInfo(const Transform& localTransform)
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

        m_orthographic.viewSpaceAABB = AxisAlignedBoundingBox3D();
    }

    glm::mat4 CameraViewInfo::viewMatrix() const
    { 
        return glm::lookAt(m_worldSpacePosition, m_worldSpacePosition + worldSpaceForward(), m_worldUp);
    }

    glm::mat4 CameraViewInfo::projectionMatrix() const
    {
        switch (m_projectionMode)
        {
        case ProjectionMode::kPerspective: 
            return glm::perspective(glm::radians(m_perspective.fov), m_perspective.aspectRatio, m_perspective.n, m_perspective.f);
        case ProjectionMode::kOrthographic:
        {
            const AxisAlignedBoundingBox3D aabb = m_orthographic.viewSpaceAABB;
            f32 left = aabb.pmin.x;
            f32 right = aabb.pmax.x;
            f32 bottom = aabb.pmin.y;
            f32 top = aabb.pmax.y;
            f32 zNear = aabb.pmax.z;
            f32 zFar = aabb.pmin.z;
            return glm::orthoLH(left, right, bottom, top, zNear, zFar);
        }
        default: assert(0); return glm::mat4(1.f);
        }
    }
}