#pragma once

#include "Core.h"
#include "MathLibrary.h"
#include "Bounds.h"
#include "Transform.h"

namespace Cyan
{
    struct CORE_API CameraViewInfo
    {
        enum class ProjectionMode
        {
            kPerspective = 0,
            kOrthographic,
            kCount
        };

        struct PerspectiveParameters
        {
            f32 n;
            f32 f; 
            f32 fov;
            f32 aspectRatio;
        };

        struct OrthographicParameters
        {
            AxisAlignedBoundingBox3D viewSpaceAABB;
        };

        CameraViewInfo();
        CameraViewInfo(const Transform& localTransform);

        static glm::vec3 worldUpVector() { return glm::vec3(0.f, 1.f, 0.f); }

        void setProjectionMode(const ProjectionMode& mode) { m_projectionMode = mode; }
        glm::mat4 viewMatrix() const;
        glm::mat4 projectionMatrix() const;

        glm::vec3 localSpaceForward() const { return m_localSpaceForward; }
        glm::vec3 localSpaceRight() const { return m_localSpaceRight; }
        glm::vec3 localSpaceUp() const { return m_localSpaceUp; }

        glm::vec3 worldSpaceForward() const { return m_worldSpaceForward; }
        glm::vec3 worldSpaceRight() const { return m_worldSpaceRight; }
        glm::vec3 worldSpaceUp() const { return m_worldSpaceUp; }

        glm::vec3 m_worldSpacePosition;
        glm::vec3 m_worldUp;

        static constexpr glm::vec3 localRight = glm::vec3(1.f, 0.f, 0.f);
        static constexpr glm::vec3 localForward = glm::vec3(0.f, 0.f, -1.f);
        static constexpr glm::vec3 localUp = glm::vec3(0.f, 1.f, 0.f);

        glm::vec3 m_localSpaceRight;   // x 
        glm::vec3 m_localSpaceForward; //-z
        glm::vec3 m_localSpaceUp;      // y

        glm::vec3 m_worldSpaceRight;   // x 
        glm::vec3 m_worldSpaceForward; //-z
        glm::vec3 m_worldSpaceUp;      // y

        ProjectionMode m_projectionMode;
        PerspectiveParameters m_perspective;
        OrthographicParameters m_orthographic;
    };
}
