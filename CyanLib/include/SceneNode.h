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

struct SceneNode
{
    // owner
    struct Entity* m_owner; 
    struct Scene*  m_scene;
    // identifier
    char m_name[kSceneNodeNameMaxLen];
    // node hierarchy
    SceneNode* m_parent;
    std::vector<SceneNode*> m_child;
    // transform component
    bool      needUpdate;
    u32       localTransform;
    u32       globalTransform;
    Transform m_localTransform;
    Transform m_worldTransform;
    // mesh component 
    Cyan::MeshInstance* m_meshInstance;
    bool m_hasAABB;
    void setParent(SceneNode* parent);
    void attach(SceneNode* child);
    void onAttach();
    void detach();
    void onDetach();
    void markChanged();
    void markUnchanged();
#if 0
    void updateWorldTransform();
#endif
    const Transform& getLocalTransform();
    const Transform& getWorldTransform();
    const glm::mat4& getLocalMatrix();
    const glm::mat4& getWorldMatrix();
    void             setLocalTransform(const Transform& mat);
    void             setWorldTransform(const Transform& mat);
    void             setLocalMatrix(const glm::mat4& mat);
    void             setWorldMatrix(const glm::mat4& mat);
    Cyan::MeshInstance* getAttachedMesh() { return m_meshInstance; }
    SceneNode* find(const char* name);
};