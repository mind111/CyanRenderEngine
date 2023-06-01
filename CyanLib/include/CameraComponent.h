#pragma once

#include <utility>

#include "Camera.h"
#include "SceneComponent.h"

namespace Cyan
{
    class CameraComponent : public SceneComponent
    {
    public:
        template <typename ... Args>
        CameraComponent(Args&&... args) 
            : SceneComponent(std::forward<Args>(args)...)
        { }

        ~CameraComponent() { }

        Camera* getCamera() { return m_camera.get(); }

    protected:
        std::unique_ptr<Camera> m_camera = nullptr;
    };
}
