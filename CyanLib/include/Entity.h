#pragma once

#include <queue>

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "Node.h"
#include "SceneNode.h"
#include "Geometry.h"

#define kEntityNameMaxLen 128u

enum EntityProperty
{
    Lit = 0,
    BakeInProbes,
};

enum EntityFilter
{
    BakeInLightMap = 0
};

// entity
/* 
    * every entity has to have a transform component, entity's transform component is represented by
    m_sceneRoot's transform.
*/
struct Entity
{
    char m_name[kEntityNameMaxLen];
    uint32_t m_entityId;

    // entity hierarchy
    Entity* m_parent;
    std::vector<Entity*> m_child;

    // scene component
    SceneNode* m_sceneRoot;

    // flags
    bool m_lit;
    bool m_static;
    bool m_visible;
    bool m_selectable;
    bool m_includeInGBufferPass;

    Entity(struct Scene* scene, const char* name, u32 id, Transform t, Entity* parent, bool isStatic);
 
    SceneNode* getSceneRoot();
    SceneNode* getSceneNode(const char* name);
    void attachSceneNode(SceneNode* child, const char* parentName=nullptr);
    bool castVisibilityRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& modelView);
    struct RayCastInfo intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& view);
    // merely sets the parent entity, it's not this method's responsibility to trigger
    // any logic relates to parent change
    void setParent(Entity* parent);
    // attach a new child Entity
    void attach(Entity* child);
    void onAttach();
    // detach from current parent
    void detach();
    i32 getChildIndex(const char* name);
    void onDetach();
    void removeChild(const char* name);
    Entity* detachChild(const char* name);
    Transform& getLocalTransform();
    void setLocalTransform(const Transform& transform);
    Transform& getWorldTransform();
    glm::vec3& getWorldPosition();
    // transform locally
    void applyLocalTransform(Transform& transform);
    void applyWorldRotation(const glm::mat4& rot);
    void applyWorldRotation(const glm::quat& rot);
    void applyWorldTranslation(const glm::vec3 trans);
    void applyWorldScale(const glm::vec3 scale);
    void updateWorldTransform();
    void applyLocalRotation(const glm::quat& rot);
    void applyLocalTranslation(const glm::vec3 trans);
    void applyLocalScale(const glm::vec3 scale);
    void setMaterial(const char* nodeName, i32 subMeshIndex, Cyan::MaterialInstance* matl);
    void setMaterial(const char* nodeName, i32 subMeshIndex, Cyan::StandardPbrMaterial* matl);
};

struct RayCastInfo
{
    // Entity* m_entity;
    SceneNode* m_node;
    i32   smIndex;
    i32   triIndex;
    float t;

    RayCastInfo() 
    : 
    //m_entity(nullptr), 
    m_node(nullptr), smIndex(-1), triIndex(-1), t(FLT_MAX)
    {} 

    bool operator<(const RayCastInfo& rhs)
    {
        return t < rhs.t;
    }
};