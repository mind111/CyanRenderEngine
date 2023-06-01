#pragma once

#include "CameraComponent.h"

namespace Cyan
{
    class PerspectiveCameraComponent : public CameraComponent
    {
    public:
        PerspectiveCameraComponent(Entity* owner, const char* name, const Transform& localTransform,
            const glm::vec3& lookAt, const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
            f32 fov, f32 n, f32 f);
        ~PerspectiveCameraComponent() { }

        virtual void onTransformUpdated() override;
    };
}
