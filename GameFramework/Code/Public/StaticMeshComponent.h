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
        StaticMeshComponent(const char* name, const Transform& localTransform, std::shared_ptr<StaticMesh> mesh);
        ~StaticMeshComponent() { }

        virtual void onTransformUpdated() override;

        StaticMeshInstance* getMeshInstance() { return m_staticMeshInstance.get(); }
        // void setMaterial(std::shared_ptr<Material> material, u32 index);
    private:
        std::unique_ptr<StaticMeshInstance> m_staticMeshInstance = nullptr;
    };
}