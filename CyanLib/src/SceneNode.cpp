#include "Scene.h"
#include "SceneNode.h"
#include "Entity.h"

namespace Cyan
{
    void SceneComponent::setParent(SceneComponent* parent)
    {
        m_parent = parent;
    }

    void SceneComponent::attachChild(SceneComponent* child)
    {
        m_child.push_back(child);
        child->setParent(this);
        child->onAttachTo();
    }

    void SceneComponent::attachIndirectChild(SceneComponent* child)
    {
        m_indirectChild.push_back(child);
        child->setParent(this);
        child->onAttachTo();
    }

    void SceneComponent::onAttachTo()
    {

    }

    const Transform& SceneComponent::getLocalTransform()
    {
        return m_scene->localTransformPool.getObject(localTransform);
    }

    const Transform& SceneComponent::getWorldTransform()
    {
        return m_scene->globalTransformPool.getObject(globalTransform);
    }

    const glm::mat4& SceneComponent::getLocalTransformMatrix()
    {
        return m_scene->localTransformMatrixPool.getObject(localTransform);
    }

    const glm::mat4& SceneComponent::getWorldTransformMatrix()
    {
        return m_scene->globalTransformMatrixPool.getObject(globalTransform);
    }

#if 0
    // basic depth first traversal
    void SceneNode::updateWorldTransform()
    {
        if (m_parent)
        {
            m_worldTransform.fromMatrix(m_parent->m_worldTransform.toMatrix() * m_localTransform.toMatrix());
        }
        else
        {
            // this node is a root node
            m_worldTransform = m_localTransform;
        }
        for (auto* child : m_child)
        {
            child->updateWorldTransform();
        }
    }
#endif

    SceneComponent* SceneComponent::find(const char* name)
    {
        return treeBFS<SceneComponent>(this, name);
    }
}
