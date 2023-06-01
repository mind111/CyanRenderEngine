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
        StaticMeshEntity(World* world, const char* name, const Transform& transform, Entity* parent, std::shared_ptr<StaticMesh> mesh);
        ~StaticMeshEntity();

        StaticMeshComponent* getStaticMeshComponent();
        void setMaterial(std::shared_ptr<Material> material, i32 index=-1);

    private:
        std::unique_ptr<StaticMeshComponent> m_staticMeshComponent = nullptr;
    };
}
