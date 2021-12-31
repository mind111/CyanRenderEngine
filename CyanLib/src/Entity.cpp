#include "Entity.h"
#include "CyanAPI.h"
#include "MathUtils.h"
#include "BVH.h"

Entity::Entity(Scene* scene, const char* name, u32 id, Transform t, Entity* parent, bool isStatic)
    : m_entityId(id), m_static(isStatic), m_includeInGBufferPass(true), m_visible(true), m_selectable(true)
{
    if (name) 
    {
        CYAN_ASSERT(strlen(name) < kEntityNameMaxLen, "Entity name too long !!")
        strcpy(m_name, name);
    } else {
        char buff[64];
        sprintf(buff, "Entity%u", m_entityId);
    }
    m_sceneRoot = Cyan::createSceneNode(scene, "DefaultSceneRoot", t);
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
    child->m_owner = this;
}

void transformRayToObjectSpace(glm::vec3& ro, glm::vec3& rd, glm::mat4& transform)
{
    auto invWorldTransform = glm::inverse(transform);
    ro = Cyan::vec4ToVec3(invWorldTransform * glm::vec4(ro, 1.f));
    rd = Cyan::vec4ToVec3(invWorldTransform * glm::vec4(rd, 0.f));
}

f32 transformHitFromObjectToWorldSpace(glm::vec3& objectSpaceHit, glm::mat4& transform, const glm::vec3& roWorldSpace, const glm::vec3& rdWorldSpace)
{
    glm::vec3 worldSpaceHit = Cyan::vec4ToVec3(transform * glm::vec4(objectSpaceHit, 1.f));
    f32 t = -1.f;
    if (rdWorldSpace.x != 0.f)
        t = (worldSpaceHit.x - roWorldSpace.x) / rdWorldSpace.x;
    else if (rdWorldSpace.y != 0.f)
        t = (worldSpaceHit.y - roWorldSpace.y) / rdWorldSpace.y;
    else if (worldSpaceHit.z != 0.f)
        t = (worldSpaceHit.z - roWorldSpace.z) / rdWorldSpace.z;
    CYAN_ASSERT(t >= 0.f, "Invalid ray hit!");
    return t;
}

// this intersection procedure does not need to find closest intersection, it 
// only returns whether there is a occlusion or not
bool Entity::castVisibilityRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& modelView)
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

            // transform the ray into object space
            glm::vec3 roObjectSpace = ro;
            glm::vec3 rdObjectSpace = rd;
            transformRayToObjectSpace(roObjectSpace, rdObjectSpace, modelView);
            rdObjectSpace = glm::normalize(rdObjectSpace);

            if (aabb.intersectRay(roObjectSpace, rdObjectSpace) > 0.f)
            {
                // do ray triangle intersectiont test
                Cyan::Mesh* mesh = node->m_meshInstance->m_mesh;
                if (mesh->bruteForceVisibilityRay(roObjectSpace, rdObjectSpace))
                    return true; 
            }
        }
        for (auto child : node->m_child)
            queue.push(child);
    }
    return false;
}

RayCastInfo Entity::intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& view)
{
    RayCastInfo globalRayHit;

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

            // transform the ray into object space
            glm::vec3 roObjectSpace = ro;
            glm::vec3 rdObjectSpace = rd;
            transformRayToObjectSpace(roObjectSpace, rdObjectSpace, modelView);
            rdObjectSpace = glm::normalize(rdObjectSpace);

            if (aabb.intersectRay(roObjectSpace, rdObjectSpace) > 0.f)
            {
                // do ray triangle intersectiont test
                Cyan::Mesh* mesh = node->m_meshInstance->m_mesh;
                Cyan::MeshRayHit currentRayHit = mesh->intersectRay(roObjectSpace, rdObjectSpace);

                if (currentRayHit.t > 0.f)
                {
                    // convert hit from object space back to world space
                    auto objectSpaceHit = roObjectSpace + currentRayHit.t * rdObjectSpace;
                    f32 currentWorldSpaceDistance = transformHitFromObjectToWorldSpace(objectSpaceHit, modelView, ro, rd);

                    if (currentWorldSpaceDistance < globalRayHit.t)
                    {
                        globalRayHit.t = currentWorldSpaceDistance;
                        globalRayHit.smIndex = currentRayHit.smIndex;
                        globalRayHit.triIndex = currentRayHit.triangleIndex;
                        // globalRayHit.m_entity = this;
                        globalRayHit.m_node = node;
                    }
                }
            }
        }

        for (auto child : node->m_child)
            queue.push(child);
    }

    if (globalRayHit.smIndex < 0 || globalRayHit.triIndex < 0)
        globalRayHit.t = -1.f;

    return globalRayHit;
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

void Entity::setMaterial(const char* nodeName, i32 subMeshIndex, Cyan::MaterialInstance* matl)
{
    if (subMeshIndex >= 0)
        getSceneNode(nodeName)->m_meshInstance->setMaterial(subMeshIndex, matl);
    else
    {
        SceneNode* sceneNode = getSceneNode(nodeName);
        for (u32 sm = 0; sm < sceneNode->m_meshInstance->m_mesh->numSubMeshes(); ++sm)
            sceneNode->m_meshInstance->setMaterial(sm, matl);
    }
}

void Entity::setMaterial(const char* nodeName, i32 subMeshIndex, Cyan::StandardPbrMaterial* matl)
{
    if (subMeshIndex >= 0)
        getSceneNode(nodeName)->m_meshInstance->setMaterial(subMeshIndex, matl->m_materialInstance);
    else
    {
        SceneNode* sceneNode = getSceneNode(nodeName);
        for (u32 sm = 0; sm < sceneNode->m_meshInstance->m_mesh->numSubMeshes(); ++sm)
            sceneNode->m_meshInstance->setMaterial(sm, matl->m_materialInstance);
    }
}
