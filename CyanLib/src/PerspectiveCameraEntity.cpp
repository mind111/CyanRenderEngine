#include "CameraEntity.h"

namespace Cyan
{
    PerspectiveCameraEntity::PerspectiveCameraEntity(World* world, const char* name, const Transform& local, const Transform& localToWorld, Entity* parent,
        const glm::vec3& lookAt, const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
        f32 fov, f32 n, f32 f)
        : Entity(world, name, local, localToWorld, parent)
    {
        // todo: deprecate "lookAt" parameter
        // todo: use lookAt to and spawn position to calculate the camera pose (mostly rotation matrix)
        m_cameraComponent = std::make_shared<PerspectiveCameraComponent>(
            "PerspectiveCameraComponent", Transform(), // SceneComponent
            lookAt, worldUp, renderResolution, viewMode,
            fov, n, f // PerspectiveCamera
            );
        m_rootSceneComponent->attachChild(m_cameraComponent);
    }

    void PerspectiveCameraEntity::update()
    {
        Entity::update();
    }
}