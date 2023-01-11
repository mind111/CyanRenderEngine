#include "Entity.h"
#include "CyanAPI.h"
#include "MathUtils.h"
#include "BVH.h"

namespace Cyan
{
    Entity::Entity(Scene* scene, const char* inName, const Transform& t, Entity* inParent, u32 inProperties)
        : name(inName), parent(nullptr), properties(inProperties)
    {
        rootSceneComponent = std::make_unique<SceneComponent>(this, "SceneRoot", t);
        if (!inParent)
        {
            if (scene->m_rootEntity)
            {
                scene->m_rootEntity->attach(this);
            }
            else
            {
                scene->m_rootEntity = this;
            }
        }
        else
        {
            inParent->attach(this);
        }
    }

    SceneComponent* Entity::getRootSceneComponent()
    {
        return rootSceneComponent.get();
    }

    Component* Entity::getComponent(const char* name)
    {
        return rootSceneComponent->find(name);
    }

#if 0
    void Entity::attachSceneComponent(SceneComponent* child, const char* parentName)
    {
        if (parentName)
        {
            SceneComponent* parent = rootSceneComponent->find(parentName);
            parent->attachChild(child);
        }
        else
        {
            rootSceneComponent->attachChild(child);
        }
        child->owner = this;
    }
#endif

    void Entity::attachComponent(Component* component, const char* parentName)
    {
        if (parentName)
        {
            auto parent = rootSceneComponent->find(parentName);
            if (parent)
            {
                parent->attach(component);
            }
            else
            {
                assert(0);
            }
        }
        else
        {
            rootSceneComponent->attach(component);
        }
    }

    void Entity::attach(Entity* child)
    {
        childs.push_back(child);
        if (child->parent)
        {
            child->parent->detach(child->name.c_str());
        }
        child->parent = this;
    }

    void Entity::attachTo(Entity* parent)
    {
        parent->attach(this);
    }

    Entity* Entity::detach(const char* name)
    {
        Entity* found = nullptr;
        if (!childs.empty())
        {
            i32 foundAt = -1;
            for (i32 i = 0; i < childs.size(); ++i)
            {
                if (childs[i]->name == name)
                {
                    foundAt = i;
                    found = childs[i];
                    found->parent = nullptr;
                    break;
                }
            }
            if (foundAt >= 0)
            {
                childs.erase(childs.begin() + foundAt);
            }
        }
        return found;
    }

#if 0
    void Entity::visit(const std::function<void(SceneComponent*)>& func)
    {
        std::queue<SceneComponent*> sceneComponents;
        sceneComponents.push(getRootSceneComponent());
        while (!sceneComponents.empty())
        {
            auto sceneComponent = sceneComponents.front();
            sceneComponents.pop();

            func(sceneComponent);

            for (u32 i = 0; i < (u32)sceneComponent->childs.size(); ++i)
            {
                sceneComponents.push(sceneComponent->childs[i]);
            }
        }
    }
#endif

    i32 Entity::getChildIndex(const char* name)
    {
        i32 index = -1;
        for (i32 i = 0; i < childs.size(); ++i)
        {
            if (strcmp(childs[i]->name.c_str(), name) == 0)
            {
                index = i;
                break;
            }
        }
        return index;
    }

    const Transform& Entity::getLocalTransform()
    {
        return rootSceneComponent->getLocalTransform();
    }

    const Transform& Entity::getWorldTransform()
    {
        return rootSceneComponent->getWorldTransform();
    }

    const glm::vec3& Entity::getWorldPosition()
    {
        return rootSceneComponent->getWorldTransform().m_translate;
    }

    void Entity::setLocalTransform(const Transform& transform)
    {
        rootSceneComponent->m_localTransform = transform;
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
        //CYAN_ASSERT(t >= 0.f, "Invalid ray hit!");
        return t;
    }

#if 0
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
                BoundingBox3D aabb = node->m_meshInstance->getAABB();

                // transform the ray into object space
                glm::vec3 roObjectSpace = ro;
                glm::vec3 rdObjectSpace = rd;
                transformRayToObjectSpace(roObjectSpace, rdObjectSpace, modelView);
                rdObjectSpace = glm::normalize(rdObjectSpace);

                if (aabb.intersectRay(roObjectSpace, rdObjectSpace) > 0.f)
                {
                    // do ray triangle intersectiont test
                    Cyan::Mesh* mesh = node->m_meshInstance->mesh;
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
                BoundingBox3D aabb = node->m_meshInstance->getAABB();

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
#endif

}
