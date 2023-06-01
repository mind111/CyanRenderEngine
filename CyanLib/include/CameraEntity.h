#pragma once

#include "PerspectiveCameraComponent.h"
#include "Entity.h"

namespace Cyan
{
    class PerspectiveCameraEntity : public Entity
    {
    public:
        PerspectiveCameraEntity(World* world, const char* name, const Transform& transform, Entity* parent,
            const glm::vec3& lookAt, const glm::vec3& worldUp, const glm::uvec2& renderResolution, const Camera::ViewMode& viewMode,
            f32 fov, f32 n, f32 f);
        ~PerspectiveCameraEntity() { }
        
        virtual void update() override;

        PerspectiveCameraComponent* getCameraComponent() { return m_cameraComponent.get(); }
    protected:
        std::unique_ptr<PerspectiveCameraComponent> m_cameraComponent = nullptr;
    private:
#if 0
        /* Camera movements */
        void moveForward()
        { 
            Camera* camera = getCamera();
            getCamera()->m_position += camera->forward() * 0.1f;
        }

        void moveLeft()
        {
            Camera* camera = getCamera();
            getCamera()->m_position -= camera->right() * 0.1f;
        }
        void moveRight()
        {
            Camera* camera = getCamera();
            getCamera()->m_position += camera->right() * 0.1f;
        }

        void moveBack()
        {
            Camera* camera = getCamera();
            getCamera()->m_position -= camera->forward() * 0.1f;
        }

        void orbit(f32 phi, f32 theta);
        void rotate();
        void zoom(f32 distance);
#endif
    };
}
