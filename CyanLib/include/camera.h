#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Common.h"
#include "Geometry.h"
#include "Transform.h"

namespace Cyan
{
    class SceneRender;

    class Camera
    {
    public:
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
            BoundingBox3D viewSpaceAABB;
        };

        Camera();
        Camera(const Transform& localTransform);
        Camera(const glm::uvec2& resolution);
        virtual ~Camera();

        static glm::vec3 worldUpVector() { return glm::vec3(0.f, 1.f, 0.f); }

        void setProjectionMode(const ProjectionMode& mode) { m_projectionMode = mode; }

        glm::mat4 viewMatrix() const
        { 
            return glm::lookAt(m_worldSpacePosition, m_worldSpacePosition + worldSpaceForward(), m_worldUp);
        }

        glm::mat4 projectionMatrix() const
        {
            switch (m_projectionMode)
            {
            case ProjectionMode::kPerspective: 
                return glm::perspective(glm::radians(m_perspective.fov), m_perspective.aspectRatio, m_perspective.n, m_perspective.f);
            case ProjectionMode::kOrthographic:
            {
                const BoundingBox3D aabb = m_orthographic.viewSpaceAABB;
                f32 left = aabb.pmin.x;
                f32 right = aabb.pmax.x;
                f32 bottom = aabb.pmin.y;
                f32 top = aabb.pmax.y;
                f32 zNear = aabb.pmax.z;
                f32 zFar = aabb.pmin.z;
                return glm::orthoLH(left, right, bottom, top, zNear, zFar);
            }
            default: assert(0);
            }
        }

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

