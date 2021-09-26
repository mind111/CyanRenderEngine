#pragma once

#include <vector>
#include <queue>

#include "glm.hpp"

#include "Node.h"
#include "Transform.h"
#include "Mesh.h" 

#define kSceneNodeNameMaxLen 64u

struct SceneNode
{
    // identifier
    char m_name[kSceneNodeNameMaxLen];

    // node hierarchy
    SceneNode* m_parent;
    std::vector<SceneNode*> m_child;
    /*
        * indirect child refers to child nodes added because another entity 
        is attached to this node's hosting entity. To seperate them from direct
        child so that when node->find() is called, it won't search child nodes among
        indirect child.
    */
    // std::vector<SceneNode*> m_indirectChild;
    // transform component
    Transform m_localTransform;
    Transform m_worldTransform;

    // mesh component 
    Cyan::MeshInstance* m_meshInstance; // TODO: make this a component, not necessarily every scene nodes wants an AABB 
    bool m_hasAABB;

    void setParent(SceneNode* parent);
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
};