#include "Entity.h"

SceneNode* Entity::getSceneRoot()
{
    return m_sceneRoot;
}

SceneNode* Entity::getSceneNode(const char* name)
{
    return m_sceneRoot->find(name);
}

void Entity::attachSceneNode(SceneNode* child, const char* parentName)
{
    if (parentName)
    {
        SceneNode* parent = m_sceneRoot->find(parentName);
        parent->attach(child);
    }
    else
    {
        m_sceneRoot->attach(child);
    }
}

RayCastInfo Entity::intersectRay(const glm::vec3& ro, const glm::vec3& rd, glm::mat4& view)
{
    std::queue<SceneNode*> queue;
    queue.push(m_sceneRoot);
    while (!queue.empty())
    {
        SceneNode* node = queue.front();
        queue.pop();
        if (node->m_meshInstance)
        {
            glm::mat4 modelView = view * node->m_worldTransform.toMatrix();
            BoundingBox3f aabb = node->m_meshInstance->getAABB();
            if (aabb.intersectRay(ro, rd, modelView) > 0.f)
            {
                // do ray triangle intersectiont test
                Cyan::Mesh* mesh = node->m_meshInstance->m_mesh;
                for (auto sm : mesh->m_subMeshes)
                {
                    for (auto& tri : sm->m_triangles)
                    {
                        Triangle t(tri);
                        float hit = t.intersectRay(ro, rd, modelView); 
                        if (hit > 0.f)
                        {
                            return { this, node, hit };
                        }
                    }
                }
            }
        }
        for (auto child : node->m_child)
        {
            queue.push(child);
        }
    }
    return { nullptr, nullptr, -1.0f };
}

// merely sets the parent entity, it's not this method's responsibility to trigger
// any logic relates to parent change
void Entity::setParent(Entity* parent)
{
    m_parent = parent;
}

// attach a new child Entity
void Entity::attach(Entity* child)
{
    child->setParent(this);
    m_child.push_back(child);
    child->onAttach();
}

void Entity::onAttach()
{
    // have the parent pointer points to parent Entity's m_sceneRoot while not
    // adding this->m_sceneRoot as a child of Entity's m_sceneRoot
    m_sceneRoot->setParent(m_parent->m_sceneRoot);
    m_sceneRoot->updateWorldTransform();
    for (auto* child : m_child)
    {
        child->onAttach();
    }
}

// detach from current parent
void Entity::detach()
{
    onDetach();
    setParent(nullptr);
}

i32 Entity::getChildIndex(const char* name)
{
    i32 index = -1;
    for (i32 i = 0; i < m_child.size(); ++i)
    {
        if (strcmp(m_child[i]->m_name, name) == 0)
        {
            index = i;
            break;
        }
    }
    return index;
}

void Entity::onDetach()
{
    m_parent->removeChild(m_name);
    m_sceneRoot->setParent(nullptr);
    m_sceneRoot->updateWorldTransform();
}

void Entity::removeChild(const char* name)
{
    m_child.erase(m_child.begin() + getChildIndex(name));
}

Entity* Entity::detachChild(const char* name)
{
    Entity* child = treeBFS<Entity>(this, name);
    child->detach();
    return child;
}

Transform& Entity::getLocalTransform()
{
    return m_sceneRoot->m_localTransform;
}

void Entity::setLocalTransform(const Transform& transform)
{
    m_sceneRoot->m_localTransform = transform;
    m_sceneRoot->updateWorldTransform();
}

Transform& Entity::getWorldTransform()
{
    return m_sceneRoot->m_worldTransform;
}

// transform locally
void Entity::applyLocalTransform(Transform& transform)
{
    m_sceneRoot->m_localTransform.m_qRot *= transform.m_qRot;
    m_sceneRoot->m_localTransform.m_translate += transform.m_translate;
    m_sceneRoot->m_localTransform.m_scale *= transform.m_scale;
    updateWorldTransform();
}

void Entity::applyWorldRotation(const glm::mat4& rot)
{
    Transform& transform = getWorldTransform(); 
    transform.fromMatrix(rot * transform.toMatrix());
}

void Entity::applyWorldRotation(const glm::quat& rot)
{
    Transform& transform = getWorldTransform(); 
    transform.m_qRot *= rot;
}

void Entity::applyWorldTranslation(const glm::vec3 trans)
{

}

void Entity::applyWorldScale(const glm::vec3 scale)
{

}

void Entity::updateWorldTransform()
{
    if (m_parent)
    {
        m_sceneRoot->m_worldTransform.fromMatrix(m_parent->m_sceneRoot->m_worldTransform.toMatrix() * m_sceneRoot->m_localTransform.toMatrix());
        m_sceneRoot->updateWorldTransform();
        for (auto child : m_child)
        {
            child->updateWorldTransform();
        }
    }
    else
    {
        m_sceneRoot->m_worldTransform = m_sceneRoot->m_localTransform;
    }
}

void Entity::applyLocalRotation(const glm::quat& rot)
{
    m_sceneRoot->m_localTransform.m_qRot *= rot;
    m_sceneRoot->updateWorldTransform();
    updateWorldTransform();
}

void Entity::applyLocalTranslation(const glm::vec3 trans)
{

}

void Entity::applyLocalScale(const glm::vec3 scale)
{

}

void Entity::setMaterial(const char* nodeName, u32 subMeshIndex, Cyan::MaterialInstance* matl)
{
    getSceneNode(nodeName)->m_meshInstance->setMaterial(subMeshIndex, matl);
}