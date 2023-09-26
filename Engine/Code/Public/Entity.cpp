#include "Entity.h"
#include "World.h"

namespace Cyan
{
    // todo: parent don't need to be passed in upon construction
    Entity::Entity(World* world, const char* name, const Transform& entityLocalTransform) 
        : m_name(name), m_world(world), m_parent(nullptr)
    {
        m_rootSceneComponent = std::make_shared<SceneComponent>("SceneRoot", entityLocalTransform);
        m_rootSceneComponent->setOwner(this);
    }

    bool Entity::isRootEntity()
    {
        return (m_name == m_world->getName());
    }

    void Entity::attachChild(std::shared_ptr<Entity> childEntity)
    {
        childEntity->detach();

        m_children.push_back(childEntity);
        childEntity->setParent(this);
        childEntity->onAttached();
    }

    void Entity::onAttached()
    {
        assert(m_parent != nullptr);
        m_parent->getRootSceneComponent()->attachChild(m_rootSceneComponent);
    }

    void Entity::detach()
    {
        if (m_parent != nullptr)
        {
            i32 childIndex = 0;
            for (auto child : m_parent->m_children)
            {
                if (child->m_name == m_name)
                {
                    m_parent->m_children.erase(m_parent->m_children.begin() + childIndex);
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
        std::queue<SceneComponent*> q;
        q.push(m_rootSceneComponent.get());
        while (!q.empty())
        {
            auto sceneComponent = q.front();
            sceneComponent->update();
            q.pop();

            for (auto child : sceneComponent->m_children)
            {
                // make sure that we don't update child entity's scene components
                if (child->getOwner() == this)
                {
                    q.push(child.get());
                }
            }
        }
        for (auto component : m_components)
        {
            component->update();
        }
    }

    void Entity::attachSceneComponent(const char* parentComponentName, std::shared_ptr<SceneComponent> componentToAttach)
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
        component->setOwner(this);
    }

    SceneComponent* Entity::findSceneComponent(const char* componentName)
    {
        return nullptr;
    }
}
