#include "SkyboxEntity.h"
#include "SkyboxComponent.h"

namespace Cyan
{
    SkyboxEntity::SkyboxEntity(World* world, const char* name, const Transform& local)
        : Entity(world, name, local)
    {
        m_skyboxComponent = std::make_shared<SkyboxComponent>("SkyboxComponent", Transform());
        m_rootSceneComponent->attachChild(m_skyboxComponent);
    }

    SkyboxEntity::~SkyboxEntity()
    {

    }

    SkyboxComponent* SkyboxEntity::getSkyboxComponent()
    {
        return m_skyboxComponent.get();
    }
}