#pragma once

#include "Entity.h"

namespace Cyan 
{
    class World;
    class StaticMesh;
    class Material;
    class StaticMeshComponent;

    class StaticMeshEntity : public Entity
    {
    public:
        StaticMeshEntity(World* world, const char* name, const Transform& local, std::shared_ptr<StaticMesh> mesh);
        ~StaticMeshEntity();

        StaticMeshComponent* getStaticMeshComponent();

    private:
        std::shared_ptr<StaticMeshComponent> m_staticMeshComponent = nullptr;
    };
}
