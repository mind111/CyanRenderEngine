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
        Transform t = getLocalToWorldTransform();
        glm::mat4 mat = t.toMatrix();
        // update camera pose
        m_camera->m_right = glm::normalize(mat * glm::vec4(Camera::localRight, 0.f));
        m_camera->m_forward = glm::normalize(mat * glm::vec4(Camera::localForward, 0.f));
        m_camera->m_up = glm::normalize(mat * glm::vec4(Camera::localUp, 0.f));
        m_camera->m_position = t.translation;
    }
}