#include "Scene.h"
#include "SceneNode.h"
#include "Entity.h"

void SceneNode::setParent(SceneNode* parent)
{
    m_parent = parent;
}

void SceneNode::attachChild(SceneNode* child)
{
    m_child.push_back(child);
    child->setParent(this);
    child->onAttachTo();
}

void SceneNode::attachIndirectChild(SceneNode* child)
{
    m_indirectChild.push_back(child);
    child->setParent(this);
    child->onAttachTo();
}

void SceneNode::onAttachTo()
{

}

#if 0
void SceneNode::markChanged()
{
    needUpdate = true;
}

void SceneNode::markUnchanged()
{
    needUpdate = false;
}
#endif

const Transform& SceneNode::getLocalTransform()
{
    return m_scene->localTransformPool.getObject(localTransform);
}

const Transform& SceneNode::getWorldTransform()
{
    return m_scene->globalTransformPool.getObject(globalTransform);
}

const glm::mat4& SceneNode::getLocalTransformMatrix()
{
    return m_scene->localTransformMatrixPool.getObject(localTransform);
}

const glm::mat4& SceneNode::getWorldTransformMatrix()
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

SceneNode* SceneNode::find(const char* name)
{
    return treeBFS<SceneNode>(this, name);
}
