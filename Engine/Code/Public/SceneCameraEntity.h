#pragma once

#include "Engine.h"
#include "Entity.h"

namespace Cyan
{
    class SceneCameraComponent;

    class ENGINE_API SceneCameraEntity : public Entity
    {
    public:
        SceneCameraEntity(World* world, const char* name, const Transform& localTransform);
        virtual ~SceneCameraEntity();

        SceneCameraComponent* getSceneCameraComponent() { return m_sceneCameraComponent.get(); }
    private:
        std::shared_ptr<SceneCameraComponent> m_sceneCameraComponent = nullptr;
    };
}
