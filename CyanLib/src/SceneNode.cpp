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

RayCastInfo SceneNode::traceRay(const glm::vec3& ro, const glm::vec3& rd)
{
    glm::mat4 modelView = m_worldTransform.toMatrix();
    BoundingBox3f aabb = m_meshInstance->getAABB();

    // transform the ray into object space
    glm::vec3 roObjectSpace = ro;
    glm::vec3 rdObjectSpace = rd;
    transformRayToObjectSpace(roObjectSpace, rdObjectSpace, modelView);
    rdObjectSpace = glm::normalize(rdObjectSpace);

    if (aabb.intersectRay(roObjectSpace, rdObjectSpace) > 0.f)
    {
        // do ray triangle intersectiont test
        Cyan::Mesh* mesh = m_meshInstance->m_mesh;
        Cyan::MeshRayHit currentRayHit = mesh->intersectRay(roObjectSpace, rdObjectSpace);

        if (currentRayHit.t > 0.f)
        {
            // convert hit from object space back to world space
            auto objectSpaceHit = roObjectSpace + currentRayHit.t * rdObjectSpace;
            f32 currentWorldSpaceDistance = transformHitFromObjectToWorldSpace(objectSpaceHit, modelView, ro, rd);
            // return { this, currentRayHit.smIndex, currentRayHit.triangleIndex, currentWorldSpaceDistance };
            return RayCastInfo();
        }
    }
}