#include "Entity.h"
#include "PerspectiveCameraComponent.h"

namespace Cyan
{
    PerspectiveCameraComponent::PerspectiveCameraComponent(const char* name, const Transform& localTransform,
        const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
        f32 fov, f32 n, f32 f)
        : CameraComponent(name, localTransform)
    {
        // upon construction, only the local transform is valid, so use local transform to instantiate 
        // a camera instance
        m_camera = std::make_unique<PerspectiveCamera>(
            localTransform, worldUp, renderResolution, viewMode,
            fov, n, f
            );
    }

    void PerspectiveCameraComponent::onTransformUpdated()
    {
        // update camera's transform relative to parent
        Transform localSpaceTransform = getLocalSpaceTransform();
        glm::mat4 localTransformMatrix = localSpaceTransform.toMatrix();
        m_camera->m_localSpaceRight = glm::normalize(localTransformMatrix * glm::vec4(Camera::localRight, 0.f));
        m_camera->m_localSpaceUp = glm::normalize(localTransformMatrix * glm::vec4(Camera::localUp, 0.f));
        m_camera->m_localSpaceForward = glm::normalize(localTransformMatrix * glm::vec4(Camera::localForward, 0.f));

        // update camera's global transform
        Transform worldSpaceTransform = getWorldSpaceTransform();
        glm::mat4 worldSpaceMatrix = worldSpaceTransform.toMatrix();
        m_camera->m_worldSpaceRight = glm::normalize(worldSpaceMatrix * glm::vec4(Camera::localRight, 0.f));
        m_camera->m_worldSpaceUp = glm::normalize(worldSpaceMatrix * glm::vec4(Camera::localUp, 0.f));
        m_camera->m_worldSpaceForward = glm::normalize(worldSpaceMatrix * glm::vec4(Camera::localForward, 0.f));
        m_camera->m_worldSpacePosition = worldSpaceTransform.translation;
    }
}