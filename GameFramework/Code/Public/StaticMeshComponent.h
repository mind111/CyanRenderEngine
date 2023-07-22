#pragma once

#include "SceneComponent.h"

namespace Cyan
{
    class Material;
    class StaticMesh;
    class StaticMeshInstance;

    class StaticMeshComponent : public SceneComponent
    {
    public:
        StaticMeshComponent(const char* name, const Transform& localTransform);
        ~StaticMeshComponent();

        virtual void onTransformUpdated() override;
        virtual void setOwner(Entity* owner) override;

        StaticMeshInstance* getMeshInstance() { return m_staticMeshInstance.get(); }
        void setStaticMesh(StaticMesh* mesh);
        // void setMaterial(std::shared_ptr<Material> material, u32 index);
    private:
        std::unique_ptr<StaticMeshInstance> m_staticMeshInstance = nullptr;
    };
}