#include "Entity.h"
#include "CyanAPI.h"
#include "MathUtils.h"

Entity::Entity(const char* name, u32 id, Transform t, Entity* parent)
    : m_entityId(id), m_bakedInProbes(true)
{
    if (name) 
    {
        CYAN_ASSERT(strlen(name) < kEntityNameMaxLen, "Entity name too long !!")
        strcpy(m_name, name);
    } else {
        char buff[64];
        sprintf(buff, "Entity%u", m_entityId);
    }
    m_sceneRoot = Cyan::createSceneNode("DefaultSceneRoot", t);
    if (parent)
    {
        parent->attach(this);
    }
}

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

// this intersection procedure does not need to find closest intersection, it 
// only returns whether there is a occlusion or not
bool Entity::intersectRay(const glm::vec3& ro, const glm::vec3& rd)
{
    std::queue<SceneNode*> queue;
    queue.push(m_sceneRoot);
    while (!queue.empty())
    {
        SceneNode* node = queue.front();
        queue.pop();
        if (node->m_meshInstance)
        {
            glm::mat4 modelView = node->m_worldTransform.toMatrix();
            BoundingBox3f aabb = node->m_meshInstance->getAABB();
            if (aabb.intersectRay(ro, rd, modelView) > 0.f)
            {
                // do ray triangle intersectiont test
                Cyan::Mesh* mesh = node->m_meshInstance->m_mesh;
                for (u32 i = 0; i < mesh->m_subMeshes.size(); ++i)
                {
                    auto sm = mesh->m_subMeshes[i];
                    u32 numTriangles = sm->m_triangles.m_numVerts / 3;
                    for (u32 j = 0; j < numTriangles; ++j)
                    {
                        Triangle tri = {
                            sm->m_triangles.m_positionArray[j * 3],
                            sm->m_triangles.m_positionArray[j * 3 + 1],
                            sm->m_triangles.m_positionArray[j * 3 + 2]
                        };

                        float hit = tri.intersectRay(ro, rd, modelView); 
                        if (hit > 0.f)
                            return true;
                    }
                }
            }
        }

        for (auto child : node->m_child)
            queue.push(child);
    }
    return false;
}

RayCastInfo Entity::intersectRay(const glm::vec3& ro, const glm::vec3& rd, glm::mat4& view)
{
    // Cyan::Toolkit::ScopedTimer cpuTimer("Entity::intersectRay()", true);
    Entity* hitEntity = nullptr;
    SceneNode* hitNode = nullptr;
    i32     smIndex  = -1;
    i32     triIndex = -1;
    float closestHit = FLT_MAX;

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
                for (u32 i = 0; i < mesh->m_subMeshes.size(); ++i)
                {
                    auto sm = mesh->m_subMeshes[i];
                    // CYAN_ASSERT(sm->m_triangles.m_numVerts % 3 == 0, "Invalid triangle mesh!");
                    u32 numTriangles = sm->m_triangles.m_numVerts / 3;
                    for (u32 j = 0; j < numTriangles; ++j)
                    {
                        Triangle tri = {
                            sm->m_triangles.m_positionArray[j * 3],
                            sm->m_triangles.m_positionArray[j * 3 + 1],
                            sm->m_triangles.m_positionArray[j * 3 + 2]
                        };

                        float hit = tri.intersectRay(ro, rd, modelView); 
                        if (hit > 0.f && hit < closestHit)
                        {
                            // compute face normal for skipping backfaced triangles
                            auto v0 = Cyan::vec4ToVec3(modelView * glm::vec4(tri.m_vertices[0], 1.f)); 
                            auto v1 = Cyan::vec4ToVec3(modelView * glm::vec4(tri.m_vertices[1], 1.f)); 
                            auto v2 = Cyan::vec4ToVec3(modelView * glm::vec4(tri.m_vertices[2], 1.f)); 

                            glm::vec3 faceNormal = glm::cross(v1 - v0, v2 - v0);
                            if (glm::dot(rd, faceNormal) < 0.f)
                            {
                                smIndex = i;
                                triIndex = j;
                                closestHit = hit;
                                hitEntity = this;
                                hitNode = node;
                            }
                        }
                    }
                }
            }
        }

        for (auto child : node->m_child)
            queue.push(child);
    }
    // cpuTimer.end();
    return { hitEntity, hitNode, smIndex, triIndex, closestHit };
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

glm::vec3& Entity::getWorldPosition()
{
    return m_sceneRoot->m_worldTransform.m_translate; 
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
