#pragma once

#include "Core.h"
#include "SceneComponent.h"
#include "Transform.h"
#include "CameraViewInfo.h"
#include "SceneCamera.h"

namespace Cyan
{
    class Entity;

    class ENGINE_API SceneCameraComponent : public SceneComponent
    {
    public:
        SceneCameraComponent(const char* name, const Transform& transform);
        ~SceneCameraComponent();

        /* SceneComponent Interface*/
        virtual void setOwner(Entity* owner) override;
        virtual void onTransformUpdated() override;

        SceneCamera* getSceneCamera() { return m_sceneCamera.get(); }
        void setRenderMode(const SceneCamera::RenderMode& renderMode);
        void setResolution(const glm::uvec2& resolution);
        void setProjectionMode(CameraViewInfo::ProjectionMode& pm);
        void setNearClippingPlane(f32 n);
        void setFarClippingPlane(f32 f);
        void setFOV(f32 fov);
    private:
        std::unique_ptr<SceneCamera> m_sceneCamera = nullptr;
    };
}
