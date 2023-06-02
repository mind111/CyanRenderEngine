#include "SceneComponent.h"
#include "Entity.h"
#include "World.h"

namespace Cyan
{
    SceneComponent::SceneComponent(const char* name, const Transform& localTransform)
        : Component(name), m_parent(nullptr), m_local(localTransform), m_localToWorld()
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
        glm::mat4 localToWorldMatrix = m_parent->getLocalToWorldTransform().toMatrix() * m_local.toMatrix();
        setLocalToWorldTransform(Transform(localToWorldMatrix));
        setOwner(m_parent->getOwner());
    }

    void SceneComponent::detachFromParent()
    {
        if (m_parent != nullptr)
        {
            i32 componentIndex = 0;
            auto& children = m_parent->m_children;
            for (auto child : children)
            {
                if (child->m_name == m_name)
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
        setLocalTransform(Transform());
    }

    void SceneComponent::setParent(SceneComponent* parent)
    {
        m_parent = parent;
    }

    void SceneComponent::setLocalTransform(const Transform& localTransform)
    {
        m_local = localTransform;
        if (m_parent != nullptr)
        {
            glm::mat4 localToWorldMatrix = m_parent->getLocalToWorldTransform().toMatrix() * m_local.toMatrix();
            setLocalToWorldTransform(localToWorldMatrix);
        }
    }

    void SceneComponent::setLocalToWorldTransform(const Transform& localToWorldTransform)
    {
        // todo: when directly setting the world transform, its localTransform also need to be updated
        m_localToWorld = localToWorldTransform;
        onTransformUpdated();
    }

    void SceneComponent::finalizeAndUpdateTransform()
    {
        if (m_parent != nullptr)
        {
            Transform localToWorld;
            localToWorld.fromMatrix(m_parent->getLocalToWorldTransform().toMatrix() * m_local.toMatrix());
            setLocalToWorldTransform(localToWorld);
        }
        else
        {
            setLocalToWorldTransform(m_local);
        }

        // propagate transform changes to children
        for (auto child : m_children)
        {
            child->finalizeAndUpdateTransform();
        }
    }
}
