#include "SceneCameraEntity.h"
#include "Transform.h"
#include "SceneCameraComponent.h"

namespace Cyan
{
    SceneCameraEntity::SceneCameraEntity(World* world, const char* name, const Transform& localTransform)
        : Entity(world, name, localTransform)
    {
        m_sceneCameraComponent = std::make_shared<SceneCameraComponent>("SceneCameraComponent", Transform());
        m_rootSceneComponent->attachChild(m_sceneCameraComponent);
    }

    SceneCameraEntity::~SceneCameraEntity()
    {

    }
}