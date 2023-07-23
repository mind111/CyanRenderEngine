#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "World.h"
#include "Entity.h"
#include "GfxInterface.h"

namespace Cyan
{
    StaticMeshComponent::StaticMeshComponent(const char* name, const Transform& localTransform)
        : SceneComponent(name, localTransform)
    {
    }
    
    StaticMeshComponent::~StaticMeshComponent()
    {
    }

    void StaticMeshComponent::onTransformUpdated()
    {
        // update transform to make sure that it's synced on Scene side
        m_staticMeshInstance->setLocalToWorldTransform(getWorldSpaceTransform());
    }

    void StaticMeshComponent::setOwner(Entity* owner)
    {
        Entity* prevOwner = owner;
        if (prevOwner != nullptr)
        {
            // todo: remove the static mesh instance from the previous scene
        }

        // update owner
        SceneComponent::setOwner(owner);

        /**
         * When a StaticMeshComponent has a owning entity, that's when it has a Scene to be added to
         */
        World* world = owner->getWorld();
        if (world != nullptr)
        {
            IScene* scene = world->getScene();
            if (scene != nullptr)
            {
                if (m_staticMeshInstance != nullptr)
                {
                    i32 instanceID = m_staticMeshInstance->getInstanceID();
                    StaticMesh& mesh = *m_staticMeshInstance->getParentMesh();
                    for (u32 i = 0; i < mesh.numSubMeshes(); ++i)
                    {
                        mesh[i]->addListener([this, scene, instanceID](StaticSubMesh* sm) {
                            // this function needs to be run on the rendering thread
                            StaticSubMeshInstance instance = { };
                            instance.subMesh = GfxStaticSubMesh::create(sm->getGeometry());
                            instance.localToWorldMatrix = m_staticMeshInstance->getLocalToWorldMatrix();
                            scene->addStaticSubMeshInstance(instance);
                        });
                    }
                }
            }
        }
    }

    void StaticMeshComponent::setStaticMesh(StaticMesh* mesh)
    {
        m_staticMeshInstance.release();
        m_staticMeshInstance = std::move(mesh->createInstance(getWorldSpaceTransform()));
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
