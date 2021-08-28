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
    Transform m_instanceTransform;
    Transform m_worldTransform;
    // glm::mat4 m_worldTransformMatrix;

    // mesh component 
    Cyan::MeshInstance* m_meshInstance;

    void setParent(SceneNode* parent);
    void attach(SceneNode* child);
    void onAttach();
    void detach();
    void onDetach();
    void updateWorldTransform();

    SceneNode* find(const char* name);
};