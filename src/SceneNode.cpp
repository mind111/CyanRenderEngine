#include "SceneNode.h"


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
    updateWorldTransform();
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

// basic depth first traversal
void SceneNode::updateWorldTransform()
{
    if (m_parent)
    {
        m_worldTransform.fromMatrix(m_parent->m_worldTransform.toMatrix() * m_instanceTransform.toMatrix());
        // m_worldTransformMatrix = m_parent->m_worldTransformMatrix * m_instanceTransform.toMatrix();
    } 
    else 
    {
        // this node is a root node
        m_worldTransform = m_instanceTransform;
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