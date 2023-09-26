#pragma once

#include "Entity.h"

namespace Cyan
{
    class StaticMeshComponent;

    class ENGINE_API StaticMeshEntity : public Entity 
    {
    public:
        StaticMeshEntity(World* world, const char* name, const Transform& entityLocalTransform);
        ~StaticMeshEntity();

        StaticMeshComponent* getStaticMeshComponent() { return m_staticMeshComponent.get(); }
    private:
        std::shared_ptr<StaticMeshComponent> m_staticMeshComponent = nullptr;
    };
}
