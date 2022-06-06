#include "Entity.h"
#include "CyanAPI.h"
#include "MathUtils.h"
#include "BVH.h"

namespace Cyan
{
    Entity::Entity(Scene* scene, const char* inName, const Transform& t, Entity* inParent, u32 inProperties)
        : name(inName),
        parent(inParent),
        properties(inProperties)
    {
        rootSceneComponent = scene->createSceneComponent("SceneRoot", t);
        rootSceneComponent->owner = this;
        if (parent)
        {
            parent->attachChild(this);
        }
    }

    SceneComponent* Entity::getRootSceneComponent()
    {
        return rootSceneComponent;
    }

    SceneComponent* Entity::getSceneComponent(const char* name)
    {
        return rootSceneComponent->find(name);
    }

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

    // merely sets the parent entity, it's not this method's responsibility to trigger
    // any logic relates to parent change
    void Entity::setParent(Entity* inParent)
    {
        parent = inParent;
    }

    void Entity::attachChild(Entity* child)
    {
        child->setParent(this);
        childs.push_back(child);
        child->onAttach();
    }

    void Entity::onAttach()
    {
        parent->rootSceneComponent->attachIndirectChild(rootSceneComponent);
    }

    void Entity::visit(const std::function<void(SceneComponent*)>& func)
    {
        std::queue<SceneComponent*> sceneComponents;
        sceneComponents.push(getRootSceneComponent());
        while (!sceneComponents.empty())
        {
            auto sceneComponent = sceneComponents.front();
            sceneComponents.pop();

            func(sceneComponent);

            for (u32 i = 0; i < (u32)sceneComponent->m_child.size(); ++i)
            {
                sceneComponents.push(sceneComponent->m_child[i]);
            }
        }
    }

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

    void Entity::setMaterial(const char* meshComponentName, i32 submeshIndex, Cyan::IMaterial* matl)
    {
        SceneComponent* sceneComponent = getSceneComponent(meshComponentName);
        if (!sceneComponent)
        {
            return;
        }
        if (submeshIndex >= 0)
        {
            auto meshInst = sceneComponent->getAttachedMesh();
            if (meshInst)
            {
                meshInst->setMaterial(matl, submeshIndex);
            }
        }
        else
        {
            auto meshInst = sceneComponent->getAttachedMesh();
            if (meshInst)
            {
                for (u32 sm = 0; sm < meshInst->parent->numSubmeshes(); ++sm)
                {
                    meshInst->setMaterial(matl, sm);
                }
            }
        }
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
