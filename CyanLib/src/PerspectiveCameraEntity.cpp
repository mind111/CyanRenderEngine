#include "CameraEntity.h"

namespace Cyan
{
    PerspectiveCameraEntity::PerspectiveCameraEntity(World* world, const char* name, const Transform& localToWorld, Entity* parent,
        const glm::vec3& lookAt, const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
        f32 fov, f32 n, f32 f)
        : Entity(world, name, localToWorld, parent)
    {
        // todo: deprecate "lookAt" parameter
        // todo: use lookAt to and spawn position to calculate the camera pose (mostly rotation matrix)
        m_cameraComponent = std::make_unique<PerspectiveCameraComponent>(
            this, "PerspectiveCameraComponent", Transform(), // SceneComponent
            lookAt, worldUp, renderResolution, viewMode,
            fov, n, f // PerspectiveCamera
            );
        m_rootSceneComponent->attachChild(m_cameraComponent.get());
    }

    void PerspectiveCameraEntity::update()
    {
        // todo: handle movement here
    }

#if 0
    void CameraEntity::orbit(f32 phi, f32 theta)
    {
        Camera* camera = getCamera();
        glm::vec3 p = camera->m_position - camera->m_lookAt;
        glm::quat quat(cos(.5f * -phi), sin(.5f * -phi) * camera->m_worldUp);
        quat = glm::rotate(quat, -theta, camera->right());
        glm::mat4 model(1.f);
        model = glm::translate(model, camera->m_lookAt);
        glm::mat4 rot = glm::toMat4(quat);
        glm::vec4 pPrime = rot * glm::vec4(p, 1.f);
        camera->m_position = glm::vec3(pPrime.x, pPrime.y, pPrime.z) + camera->m_lookAt;
    }

    void CameraEntity::rotate()
    {

    }

    void CameraEntity::zoom(f32 distance)
    {
        Camera* camera = getCamera();
        glm::vec3 forward = camera->forward();
        glm::vec3 translation = forward * distance;
        glm::vec3 v1 = glm::normalize(camera->m_position + translation - camera->m_lookAt); 
        if (glm::dot(v1, forward) >= 0.f)
        {
            camera->m_position = camera->m_lookAt - 0.001f * forward;
        }
        else
        {
            camera->m_position += translation;
        }
    }
#endif
}