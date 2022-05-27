#pragma once

#include <queue>
#include <functional>

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "Node.h"
#include "SceneNode.h"
#include "Geometry.h"

#define kEntityNameMaxLen 128u

enum EntityProperty
{
    kLit = 0 << 1,
    kVisible = 1 << 1,
    kStatic = 2 << 1,
    kSelectable = 3 << 1,
    kIncludeInGBufferPass = 4 << 1,
    kCastShadow = 5 << 1,
    BakeInProbes = 6 << 1,
};

enum class EntityFilter
{
    BakeInLightMap = 0,
    kAll
};

void transformRayToObjectSpace(glm::vec3& ro, glm::vec3& rd, glm::mat4& transform);
f32  transformHitFromObjectToWorldSpace(glm::vec3& objectSpaceHit, glm::mat4& transform, const glm::vec3& roWorldSpace, const glm::vec3& rdWorldSpace);


#define EntityFlag_kStatic (u32)Entity::Mobility::kStatic << 1
#define EntityFlag_kDynamic (u32)Entity::Mobility::kDynamic << 1
#define EntityFlag_kVisible (u32)Entity::Visibility::kVisible << 1
#define EntityFlag_kCastShadow (u32)Entity::Lighting::kCastShadow << 1

// forward declare
// entity
/* 
    * every entity has to have a transform component, entity's transform component is represented by
    m_sceneRoot's transform.
*/
struct Entity
{
    enum class Mobility
    {
        kStatic = 0,
        kDynamic,
        kCount
    };

    enum class Visibility
    {
        kVisible = (u32)Mobility::kCount,
        kCount
    };
    
    enum class Lighting
    {
        kCastShadow = (u32)Visibility::kCount,
        kCount
    };

    char m_name[kEntityNameMaxLen];
    uint32_t m_entityId;
    Entity* m_parent;
    std::vector<Entity*> m_child;
    SceneComponent* m_sceneRoot;

    // flags
    u32 flags;
    bool m_lit;
    bool m_static;
    bool m_visible;
    bool m_selectable;
    bool m_includeInGBufferPass;

    Entity(struct Scene* scene, const char* name, u32 id, Transform t, Entity* parent, bool isStatic);

    virtual void update() { }

    u32 getFlags() { return flags; }
    SceneComponent* getSceneRoot();
    SceneComponent* getSceneNode(const char* name);
    void attachSceneNode(SceneComponent* child, const char* parentName=nullptr);
    // bool castVisibilityRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& modelView);
    // struct RayCastInfo intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& view);
    // merely sets the parent entity, it's not this method's responsibility to trigger
    // any logic relates to parent change
    void setParent(Entity* parent);
    // attachChild a new child Entity
    void attachChild(Entity* child);
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
    void setMaterial(const char* meshNodeName, i32 submeshIndex, Cyan::IMaterial* matl);
};

struct RayCastInfo
{
    SceneComponent* m_node;
    i32        smIndex;
    i32        triIndex;
    f32        t;

    RayCastInfo()
    : 
    m_node(nullptr), smIndex(-1), triIndex(-1), t(FLT_MAX)
    {} 

    bool operator<(const RayCastInfo& rhs)
    {
        return t < rhs.t;
    }
};

namespace Cyan
{
    void visitEntity(Entity* e, const std::function<void(SceneComponent*)>& func);
}