#pragma once
#include <vector>
#include <queue>

#include "glm.hpp"

#include "Node.h"
#include "Transform.h"
#include "Mesh.h" 

#define kSceneNodeNameMaxLen 128u

enum SceneNodeFlag
{
    HasAABB    = 0 << 1,
    CastShadow = 1 << 1,
    NeedUpdate = 2 << 1,
};

struct SceneComponent
{
    // owner
    struct Entity* m_owner; 
    struct Scene*  m_scene;
    // identifier
    char m_name[kSceneNodeNameMaxLen];
    // node hierarchy
    SceneComponent* m_parent;
    std::vector<SceneComponent*> m_child;
    std::vector<SceneComponent*> m_indirectChild;
    // transform component
    u32       localTransform;
    u32       globalTransform;
    Transform m_localTransform;
    Transform m_worldTransform;
    // mesh component 
    // Cyan::MeshInstance* m_meshInstance;
    void setParent(SceneComponent* parent);
    void attachChild(SceneComponent* child);
    void attachIndirectChild(SceneComponent* child);
    void onAttachTo();

    virtual Cyan::MeshInstance* getAttachedMesh() { return nullptr; }

#if 0
    void markChanged();
    void markUnchanged();
    void updateWorldTransform();
#endif

    const Transform& getLocalTransform();
    const Transform& getWorldTransform();
    const glm::mat4& getLocalTransformMatrix();
    const glm::mat4& getWorldTransformMatrix();

    SceneComponent* find(const char* name);
};

struct MeshComponent : public SceneComponent
{
    Cyan::MeshInstance* meshInst = nullptr;

    virtual Cyan::MeshInstance* getAttachedMesh() { return meshInst; }
};

namespace Cyan
{

}