#pragma once

#include "Core.h"
#include "SceneComponent.h"
#include "Transform.h"

namespace Cyan
{
    class SceneCamera;
    class Entity;

    class SceneCameraComponent : public SceneComponent
    {
    public:
        SceneCameraComponent(const char* name, const Transform& transform);
        ~SceneCameraComponent();

        virtual void setOwner(Entity* owner) override;

        void setResolution(const glm::uvec2& resolution);
    private:
        std::unique_ptr<SceneCamera> m_sceneCamera = nullptr;
    };
}
