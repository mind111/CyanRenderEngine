#include "Engine.h"
#include "GfxModule.h"
#include "Texture.h"
#include "SkyboxComponent.h"
#include "SkyboxEntity.h"

namespace Cyan
{
    SkyboxEntity::SkyboxEntity(World* world, const char* name, const Transform& entityLocalTransform)
        : Entity(world, name, entityLocalTransform)
    {
        m_skyboxComponent = std::make_shared<SkyboxComponent>("SkyboxComponent", Transform());
        m_rootSceneComponent->attachChild(m_skyboxComponent);
    }

    SkyboxEntity::~SkyboxEntity() { }
}