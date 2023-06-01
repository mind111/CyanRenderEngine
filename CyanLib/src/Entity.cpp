#include "Entity.h"
#include "CyanAPI.h"
#include "MathUtils.h"
#include "BVH.h"
#include "World.h"

namespace Cyan
{
    Entity::Entity(World* world, const char* name, const Transform& localToWorld, Entity* parent) 
        : m_name(name), m_world(world), m_parent(parent)
    {
        m_rootSceneComponent = std::make_unique<SceneComponent>(this, "SceneRoot", localToWorld);
        if (m_parent != nullptr)
        {
            m_parent->attach(this);
        }
        else
        {
            assert(isRootEntity());
        }
    }

    bool Entity::isRootEntity()
    {
        return (m_name == m_world->m_name);
    }

    void Entity::attach(Entity* childEntity)
    {
        childEntity->detach();

        m_children.push_back(childEntity);
        childEntity->setParent(this);
        childEntity->onAttached();
    }

    void Entity::onAttached()
    {
        assert(m_parent != nullptr);
        m_parent->getRootSceneComponent()->attachChild(m_rootSceneComponent.get());
    }

    void Entity::detach()
    {
        if (m_parent)
        {
            i32 childIndex = 0;
            for (auto child : m_parent->m_children)
            {
                if (child->m_name == m_name)
                {
                    m_parent->m_children.erase(m_children.begin() + childIndex);
                    setParent(nullptr);
                    onDetached();
                    break;
                }

                childIndex++;
            }
        }
    }

    void Entity::onDetached()
    {
        m_rootSceneComponent->detachFromParent();
    }

    void Entity::setParent(Entity* parent)
    {
        m_parent = parent;
    }

    void Entity::update() 
    { 
    }

    void Entity::attachSceneComponent(const char* parentComponentName, SceneComponent* componentToAttach)
    {
        auto parentSceneComponent = findSceneComponent(parentComponentName);
        if (parentSceneComponent != nullptr)
        {
            parentSceneComponent->attachChild(componentToAttach);
        }
    }

    void Entity::addComponent(std::shared_ptr<Component> component) 
    { 
        m_components.push_back(component);
    }

    SceneComponent* Entity::findSceneComponent(const char* componentName)
    {
        return nullptr;
    }
}
