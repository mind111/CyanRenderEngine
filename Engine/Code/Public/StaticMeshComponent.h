#pragma once

#include "Engine.h"
#include "SceneComponent.h"

namespace Cyan
{
    class MaterialInstance;
    class StaticMesh;
    class StaticMeshInstance;

    class ENGINE_API StaticMeshComponent : public SceneComponent
    {
    public:
        StaticMeshComponent(const char* name, const Transform& localTransform);
        ~StaticMeshComponent();

        virtual void onTransformUpdated() override;
        virtual void setOwner(Entity* owner) override;

        StaticMeshInstance* getMeshInstance() { return m_staticMeshInstance.get(); }
        void setStaticMesh(StaticMesh* mesh);
        void setMaterial(i32 slot, MaterialInstance* mi);
    private:
        std::unique_ptr<StaticMeshInstance> m_staticMeshInstance = nullptr;
    };
}