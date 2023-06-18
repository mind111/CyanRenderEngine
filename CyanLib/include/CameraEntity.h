#pragma once

#include "CameraComponent.h"
#include "Entity.h"

namespace Cyan
{
    class CameraEntity : public Entity
    {
    public:
        CameraEntity(World* world, const char* name, const Transform& local, const glm::uvec2& resolution);
        ~CameraEntity() { }
        
        virtual void update() override;

        CameraComponent* getCameraComponent() { return m_cameraComponent.get(); }
    protected:
        std::shared_ptr<CameraComponent> m_cameraComponent = nullptr;
    private:
#if 0
        void orbit(f32 phi, f32 theta);
        void rotate();
        void zoom(f32 distance);
#endif
    };
}
