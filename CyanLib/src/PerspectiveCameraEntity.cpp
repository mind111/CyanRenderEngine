#include "CameraEntity.h"

namespace Cyan
{
    PerspectiveCameraEntity::PerspectiveCameraEntity(World* world, const char* name, const Transform& local,
        const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
        f32 fov, f32 n, f32 f)
        : Entity(world, name, local)
    {
        m_cameraComponent = std::make_shared<PerspectiveCameraComponent>(
            "PerspectiveCameraComponent", Transform(), // SceneComponent
            worldUp, renderResolution, viewMode,
            fov, n, f // PerspectiveCamera
            );
        m_rootSceneComponent->attachChild(m_cameraComponent);
    }

    void PerspectiveCameraEntity::update()
    {
        Entity::update();
    }
}