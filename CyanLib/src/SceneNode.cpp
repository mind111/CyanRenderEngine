#include "Scene.h"
#include "SceneNode.h"
#include "Entity.h"

namespace Cyan
{
    SceneComponent::SceneComponent(Entity* inOwner, const char* inName, const Transform& inTransform)
        : Component(inOwner, inName), m_localTransform(inTransform), m_worldTransform()
    {
    }

    SceneComponent::SceneComponent(Entity* inOwner, Component* inParent, const char* inName, const Transform& inTransform)
        : Component(inOwner, inParent, inName), m_localTransform(inTransform), m_worldTransform()
    {
    }

    const Transform& SceneComponent::getLocalTransform()
    {
        return m_localTransform;
    }

    const Transform& SceneComponent::getWorldTransform()
    {
        return m_worldTransform;
    }

    const glm::mat4& SceneComponent::getLocalTransformMatrix()
    {
        return m_localTransform.toMatrix();
    }

    const glm::mat4& SceneComponent::getWorldTransformMatrix()
    {
        return m_worldTransform.toMatrix();
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

    void MeshComponent::setMaterial(Material* material) {
        meshInst->setMaterial(material);
    }

    void MeshComponent::setMaterial(Material* material, u32 submeshIndex) {
        meshInst->setMaterial(material, submeshIndex);
    }
}
