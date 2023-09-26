#include "StaticMeshComponent.h"
#include "StaticMesh.h"
#include "World.h"
#include "Entity.h"
#include "GfxModule.h"

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
        if (m_staticMeshInstance != nullptr)
        {
            m_staticMeshInstance->setLocalToWorldTransform(getWorldSpaceTransform());
        }
    }

    void StaticMeshComponent::setOwner(Entity* owner)
    {
        Entity* prevOwner = owner;
        if (prevOwner != nullptr)
        {
            // todo: remove the static mesh instance from the previous scene
            World* world = prevOwner->getWorld();
            if (world != nullptr)
            {
                Scene* scene = world->getScene();
                if (scene != nullptr)
                {
                    if (m_staticMeshInstance != nullptr)
                    {
                        m_staticMeshInstance->removeFromScene(scene);
                    }
                }
            }
        }

        // update owner
        SceneComponent::setOwner(owner);

        /**
         * When a StaticMeshComponent has a owning entity, that's when it has a Scene to be added to
         */
        World* world = owner->getWorld();
        if (world != nullptr)
        {
            Scene* scene = world->getScene();
            if (scene != nullptr)
            {
                if (m_staticMeshInstance != nullptr)
                {
                    m_staticMeshInstance->addToScene(scene);
                }
            }
        }
    }

    void StaticMeshComponent::setStaticMesh(StaticMesh* mesh)
    {
        // detect if mesh changed
        bool bMeshChanged = false;
        StaticMesh* currentMesh = nullptr;

        if (m_staticMeshInstance != nullptr)
        {
            currentMesh = m_staticMeshInstance->getParentMesh();
        }

        if (currentMesh != nullptr)
        {
            if (mesh != nullptr)
            {
                if (currentMesh->getName() != mesh->getName())
                {
                    bMeshChanged = true;
                }
            }
        }
        else
        {
            if (mesh != nullptr)
            {
                bMeshChanged = true;
            }
        }

        if (bMeshChanged)
        {
            World* world = m_owner->getWorld();
            if (world != nullptr)
            {
                Scene* scene = world->getScene();
                if (scene != nullptr)
                {
                    if (currentMesh != nullptr)
                    {
                        m_staticMeshInstance->removeFromScene(scene);
                    }
                    else
                    {

                    }
                    m_staticMeshInstance.release();
                    m_staticMeshInstance = std::move(mesh->createInstance(getWorldSpaceTransform()));
                    m_staticMeshInstance->addToScene(scene);
                }
            }
        }
    }

    void StaticMeshComponent::setMaterial(i32 slot, MaterialInstance* mi)
    {
        if (slot < 0)
        {
            auto mesh = m_staticMeshInstance->getParentMesh();
            for (u32 i = 0; i < mesh->numSubMeshes(); ++i)
            {
                setMaterial(i, mi);
            }
        }
        else
        {
            m_staticMeshInstance->setMaterial(slot, mi);
        }
    }
}
