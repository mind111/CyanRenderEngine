#include "StaticMeshComponent.h"
#include "StaticMesh.h"

namespace Cyan
{
    StaticMeshComponent::StaticMeshComponent(const char* name, const Transform& localTransform, std::shared_ptr<StaticMesh> mesh)
        : SceneComponent(name, localTransform)
    {
        m_staticMeshInstance = mesh->createInstance(getWorldSpaceTransform().toMatrix());
    }

    void StaticMeshComponent::onTransformUpdated()
    {
        // update transform to make sure that it's synced on Scene side
        m_staticMeshInstance->setLocalToWorldTransform(getWorldSpaceTransform());
    }

#if 0
    void StaticMeshComponent::setMaterial(std::shared_ptr<Material> material, u32 index)
    {
        if (index < 0)
        {
            auto mesh = m_staticMeshInstance->getParentMesh();
            for (i32 i = 0; i < mesh->numSubMeshes(); ++i)
            {
                setMaterial(material, i);
            }
        }
        else
        {
            m_staticMeshInstance->setMaterial(material, index);
        }
    }
#endif
}
