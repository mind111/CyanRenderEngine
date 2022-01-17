#include "SceneNode.h"
#include "Entity.h"

void SceneNode::setParent(SceneNode* parent)
{
    m_parent = parent;
}

void SceneNode::attach(SceneNode* child)
{
    m_child.push_back(child);
    child->setParent(this);
    child->onAttach();
}

void SceneNode::onAttach()
{
    toggleToUpdate();
    updateWorldTransform();
}

void SceneNode::toggleToUpdate()
{
    needUpdate = !needUpdate;
}

void SceneNode::detach()
{
    m_parent = nullptr;
    onDetach();
}

void SceneNode::onDetach()
{
    updateWorldTransform();
}

const Transform& SceneNode::getLocalTransform()
{
    return m_localTransform; 
}

const Transform& SceneNode::getWorldTransform()
{
    return m_worldTransform; 
}

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

SceneNode* SceneNode::find(const char* name)
{
    return treeBFS<SceneNode>(this, name);
}
