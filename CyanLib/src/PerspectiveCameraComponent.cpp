#include "Entity.h"
#include "PerspectiveCameraComponent.h"

namespace Cyan
{
    PerspectiveCameraComponent::PerspectiveCameraComponent(Entity* owner, const char* name, const Transform& localTransform,
        const glm::vec3& lookAt, const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
        f32 fov, f32 n, f32 f)
        : CameraComponent(owner, name, localTransform)
    {
        // use this component's transform to derive position and pose
        glm::vec3 position = getLocalToWorldTransform().m_translate;
        m_camera = std::make_unique<PerspectiveCamera>(
            position, lookAt, worldUp, renderResolution, viewMode,
            fov, n, f
            );
    }

    void PerspectiveCameraComponent::onTransformUpdated()
    {
        glm::vec3 position = getLocalToWorldTransform().m_translate;
        m_camera->m_position = position;
    }
}