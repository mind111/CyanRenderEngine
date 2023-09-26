#include "SceneComponent.h"
#include "Entity.h"
#include "World.h"

namespace Cyan
{
    SceneComponent::SceneComponent(const char* name, const Transform& localTransform)
        : Component(name), m_parent(nullptr), m_localSpaceTransform(localTransform), m_worldSpaceTransform()
    {
    }

    void SceneComponent::attachToParent(SceneComponent* parent)
    { 
        parent->attachChild(std::shared_ptr<SceneComponent>(this));
    }

    void SceneComponent::attachChild(std::shared_ptr<SceneComponent> child)
    {
        child->detachFromParent();

        m_children.push_back(child);
        child->setParent(this);
        child->onAttached();
    }

    void SceneComponent::onAttached()
    {
        assert(m_parent != nullptr);
        glm::mat4 localToWorldMatrix = m_parent->getWorldSpaceTransform().toMatrix() * m_localSpaceTransform.toMatrix();
        setWorldSpaceTransform(Transform(localToWorldMatrix));
        // todo: a hack to prevent updating owner when this component is the scene root, this works for now, 
        // but really need a proper fix later
        Entity* currentOwner = getOwner();
        if (currentOwner != nullptr)
        {
            if (this != currentOwner->getRootSceneComponent())
            {
                setOwner(m_parent->getOwner());
            }
        }
        else
        {
            setOwner(m_parent->getOwner());
        }
    }

    // todo: this is bugged
    void SceneComponent::detachFromParent()
    {
        if (m_parent != nullptr)
        {
            i32 componentIndex = 0;
            auto& children = m_parent->m_children;
            for (auto child : children)
            {
#if 0
                if (child->m_name == m_name)
#else
                // instead of comparing name, compare using address in memory for now since component
                // name is not guaranteed to be unique at the moment.
                if (child.get() == this)
#endif
                {
                    children.erase(children.begin() + componentIndex);
                    setParent(nullptr);
                    onDetached();
                    break;
                }
                componentIndex++;
            }
        }
    }

    void SceneComponent::onDetached()
    {
        setLocalSpaceTransform(Transform());
    }

    void SceneComponent::setParent(SceneComponent* parent)
    {
        m_parent = parent;
    }

    void SceneComponent::setLocalSpaceTransform(const Transform& localTransform)
    {
        m_localSpaceTransform = localTransform;
        if (m_parent != nullptr)
        {
            glm::mat4 localToWorldMatrix = m_parent->getWorldSpaceTransform().toMatrix() * m_localSpaceTransform.toMatrix();
            setWorldSpaceTransform(localToWorldMatrix);
        }
    }

    void SceneComponent::setWorldSpaceTransform(const Transform& localToWorldTransform)
    {
        // todo: when directly setting the world transform, its localTransform also need to be updated
        m_worldSpaceTransform = localToWorldTransform;
        onTransformUpdated();
    }

    void SceneComponent::finalizeAndUpdateTransform()
    {
        if (m_parent != nullptr)
        {
            Transform localToWorld;
            localToWorld.fromMatrix(m_parent->getWorldSpaceTransform().toMatrix() * m_localSpaceTransform.toMatrix());
            setWorldSpaceTransform(localToWorld);
        }
        else
        {
            setWorldSpaceTransform(m_localSpaceTransform);
        }

        // propagate transform changes to children
        for (auto child : m_children)
        {
            child->finalizeAndUpdateTransform();
        }
    }

    void SceneComponent::setOwner(Entity* owner)
    {
        Component::setOwner(owner);
        for (auto child : m_children)
        {
            child->setOwner(owner);
        }
    }
}
