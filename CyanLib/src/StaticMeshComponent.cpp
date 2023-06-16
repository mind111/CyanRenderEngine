#include "StaticMeshComponent.h"

namespace Cyan
{
    StaticMeshComponent::StaticMeshComponent(const char* name, const Transform& localTransform, std::shared_ptr<StaticMesh> mesh)
        : SceneComponent(name, localTransform)
    {
        m_staticMeshInstance = std::make_shared<StaticMesh::Instance>(this, mesh, getWorldSpaceTransform());
    }

    void StaticMeshComponent::onTransformUpdated()
    {
        // update transform to make sure that it's synced on Scene side
        m_staticMeshInstance->localToWorld = getWorldSpaceTransform();
    }

    void StaticMeshComponent::setMaterial(std::shared_ptr<Material> material, u32 index)
    {
        if (index < 0)
        {
            auto mesh = m_staticMeshInstance->parent;
            for (i32 i = 0; i < mesh->getNumSubmeshes(); ++i)
            {
                setMaterial(material, i);
            }
        }
        else
        {
            (*m_staticMeshInstance)[index] = material;
        }
    }
}
