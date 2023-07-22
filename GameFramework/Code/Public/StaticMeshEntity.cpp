#include "StaticMeshEntity.h"
#include "StaticMeshComponent.h"

namespace Cyan
{

    StaticMeshEntity::StaticMeshEntity(World* world, const char* name, const Transform& entityLocalTransform)
        : Entity(world, name, entityLocalTransform)
    {
        Transform componentLocalTransform = Transform();
        m_staticMeshComponent = std::make_unique<StaticMeshComponent>("StaticMeshComponent", componentLocalTransform);
        m_staticMeshComponent->setOwner(this);
    }

    StaticMeshEntity::~StaticMeshEntity() 
    {

    }
}