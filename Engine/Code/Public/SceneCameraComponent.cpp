#include "SceneCameraComponent.h"
#include "SceneCamera.h"
#include "Entity.h"
#include "World.h"

namespace Cyan
{
    SceneCameraComponent::SceneCameraComponent(const char* name, const Transform& transform)
        : SceneComponent(name, transform)
    {
        m_sceneCamera = std::make_unique<SceneCamera>();
    }

    SceneCameraComponent::~SceneCameraComponent()
    {

    }

    void SceneCameraComponent::setOwner(Entity* owner)
    {
        SceneComponent::setOwner(owner);

        World* world = m_owner->getWorld();
        if (world != nullptr)
        {
            world->addSceneCamera(m_sceneCamera.get());
        }
    }

    void SceneCameraComponent::setResolution(const glm::uvec2& resolution)
    {

    }
}