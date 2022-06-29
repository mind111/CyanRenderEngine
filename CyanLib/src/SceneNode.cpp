#include "Scene.h"
#include "SceneNode.h"
#include "Entity.h"

namespace Cyan
{
    void SceneComponent::attachChild(SceneComponent* child)
    {
        child->onAttachTo(this);
        childs.push_back(child);
    }

    void SceneComponent::attachIndirectChild(SceneComponent* inChild)
    {
        inChild->onAttachTo(this);
        indirectChilds.push_back(inChild);
    }

    void SceneComponent::onAttachTo(SceneComponent* inParent)
    {
        if (parent)
        {
            parent->removeChild(this);
        }
        parent = inParent;
    }

    void SceneComponent::removeChild(SceneComponent* inChild)
    {
        i32 found = -1;
        i32 index = 0;
        for (i32 i = 0; i < childs.size(); ++i)
        {
            if (childs[i]->name == inChild->name)
            {
                found = index;
                break;
            }
        }
        if (found >= 0)
        {
            childs[found]->onBeingRemoved();
            childs.erase(childs.begin() + found);
        }
    }

    void SceneComponent::removeIndirectChild(SceneComponent* inChild)
    {
        i32 found = -1;
        i32 index = 0;
        for (i32 i = 0; i < indirectChilds.size(); ++i)
        {
            if (indirectChilds[i]->name == inChild->name)
            {
                found = index;
                break;
            }
        }
        if (found >= 0)
        {
            indirectChilds[found]->onBeingRemoved();
            indirectChilds.erase(indirectChilds.begin() + found);
        }
    }

    void SceneComponent::onBeingRemoved()
    {
        parent = nullptr;
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

    void MeshComponent::setMaterial(IMaterial* material)
    {
        meshInst->setMaterial(material);
    }

    void MeshComponent::setMaterial(IMaterial* material, u32 submeshIndex)
    {
        meshInst->setMaterial(material, submeshIndex);
    }
}
