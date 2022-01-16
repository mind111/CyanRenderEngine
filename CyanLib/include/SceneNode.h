#pragma once

#include <vector>
#include <queue>

#include "glm.hpp"

#include "Node.h"
#include "Transform.h"
#include "Mesh.h" 

#define kSceneNodeNameMaxLen 128u

enum SceneNodeProperty
{
    AABB = 0,
    CastShadow,
};

struct SceneNode
{
    // owner
    struct Entity* m_owner; 
    // identifier
    char m_name[kSceneNodeNameMaxLen];
    // node hierarchy
    SceneNode* m_parent;
    std::vector<SceneNode*> m_child;
    // transform component
    u32       localTransform;
    u32       globalTransform;
    Transform m_localTransform;
    Transform m_worldTransform;
    // mesh component 
    Cyan::MeshInstance* m_meshInstance;
    bool m_hasAABB;
    bool m_castShadow;
    void setParent(SceneNode* parent);
    void setCastShadw(bool& castShadow)
    {
        m_castShadow = castShadow;
    }
    void attach(SceneNode* child);
    void onAttach();
    void detach();
    void onDetach();
    void updateWorldTransform();
    const Transform& getLocalTransform();
    void setLocalTransform(glm::mat4 mat)
    {
        m_localTransform.fromMatrix(mat);
        // TODO: can we defer this update to gain performance?
        updateWorldTransform();
    }
    Cyan::MeshInstance* getAttachedMesh() { return m_meshInstance; }
    const Transform& getWorldTransform();
    SceneNode* find(const char* name);
    struct RayCastInfo traceRay(const glm::vec3& ro, const glm::vec3& rd);
};