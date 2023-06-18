#include "CameraEntity.h"

namespace Cyan
{
    CameraEntity::CameraEntity(World* world, const char* name, const Transform& local, const glm::uvec2& resolution)
        : Entity(world, name, local)
    {
        m_cameraComponent = std::make_shared<CameraComponent>("CameraComponent", Transform(), resolution);
        m_rootSceneComponent->attachChild(m_cameraComponent);
    }

    void CameraEntity::update()
    {
        Entity::update();
    }
}