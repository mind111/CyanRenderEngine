#include "Entity.h"
#include "LightEntities.h"
#include "LightComponents.h"

namespace Cyan
{
    DirectionalLightEntity::DirectionalLightEntity(World* world, const char* name, const Transform& local)
        : Entity(world, name, local)
    {
        m_directionalLightComponent = std::make_shared<DirectionalLightComponent>("DirectionalLightComponent", Transform());
        m_rootSceneComponent->attachChild(m_directionalLightComponent);
    }

    DirectionalLightEntity::~DirectionalLightEntity()
    {

    }

    DirectionalLightComponent* DirectionalLightEntity::getDirectionalLightComponent()
    {
        return m_directionalLightComponent.get();
    }

#if 0
    SkyLightEntity::SkyLightEntity(World* world, const char* name, const Transform& local)
        : Entity(world, name, local)
    {
        m_skyLightComponent = std::make_shared<SkyLightComponent>("SkyLightComponent", Transform());
        m_rootSceneComponent->attachChild(m_skyLightComponent);
    }

    SkyLightEntity::~SkyLightEntity()
    {

    }

    SkyLightComponent* SkyLightEntity::getSkyLightComponent()
    {
        return m_skyLightComponent.get();
    }
#endif
}
