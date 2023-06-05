#pragma once

#include "PerspectiveCameraComponent.h"
#include "Entity.h"

namespace Cyan
{
    class PerspectiveCameraEntity : public Entity
    {
    public:
        PerspectiveCameraEntity(World* world, const char* name, const Transform& local,
            const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
            f32 fov, f32 n, f32 f);
        ~PerspectiveCameraEntity() { }
        
        virtual void update() override;

        PerspectiveCameraComponent* getCameraComponent() { return m_cameraComponent.get(); }
    protected:
        std::shared_ptr<PerspectiveCameraComponent> m_cameraComponent = nullptr;
    private:
#if 0
        void orbit(f32 phi, f32 theta);
        void rotate();
        void zoom(f32 distance);
#endif
    };
}
