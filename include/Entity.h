#pragma once

#include <queue>

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "Node.h"
#include "SceneNode.h"

#define kEntityNameMaxLen 64u

// TODO: Components

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

    SceneNode* getSceneRoot()
    {
        return m_sceneRoot;
    }

    SceneNode* getSceneNode(const char* name)
    {
        return m_sceneRoot->find(name);
    }

    // merely sets the parent entity, it's not this method's responsibility to trigger
    // any logic relates to parent change
    void setParent(Entity* parent)
    {
        m_parent = parent;
    }

    // attach a new child Entity
    void attach(Entity* child)
    {
        child->setParent(this);
        m_child.push_back(child);
        child->onAttach();
    }

    void onAttach()
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
    void detach()
    {
        onDetach();
        setParent(nullptr);
    }

    i32 getChildIndex(const char* name)
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

    void onDetach()
    {
        m_parent->removeChild(m_name);
        m_sceneRoot->setParent(nullptr);
        m_sceneRoot->updateWorldTransform();
    }

    void removeChild(const char* name)
    {
        m_child.erase(m_child.begin() + getChildIndex(name));
    }

    Entity* detachChild(const char* name)
    {
        Entity* child = treeBFS<Entity>(this, name);
        child->detach();
        return child;
    }

    Transform& getLocalTransform()
    {
        return m_sceneRoot->m_localTransform;
    }

    Transform& getWorldTransform()
    {
        return m_sceneRoot->m_worldTransform;
    }

    // transform locally
    void applyLocalTransform(Transform& transform)
    {
        m_sceneRoot->m_localTransform.m_qRot *= transform.m_qRot;
        m_sceneRoot->m_localTransform.m_translate += transform.m_translate;
        m_sceneRoot->m_localTransform.m_scale *= transform.m_scale;
        updateWorldTransform();
    }

    void applyWorldRotation(const glm::mat4& rot)
    {
        Transform& transform = getWorldTransform(); 
        transform.fromMatrix(rot * transform.toMatrix());
    }

    void applyWorldRotation(const glm::quat& rot)
    {
        Transform& transform = getWorldTransform(); 
        transform.m_qRot *= rot;
    }

    void applyWorldTranslation(const glm::vec3 trans)
    {

    }

    void applyWorldScale(const glm::vec3 scale)
    {

    }

    void updateWorldTransform()
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

    void applyLocalRotation(const glm::quat& rot)
    {
        m_sceneRoot->m_localTransform.m_qRot *= rot;
        m_sceneRoot->updateWorldTransform();
        updateWorldTransform();
    }

    void applyLocalTranslation(const glm::vec3 trans)
    {

    }

    void applyLocalScale(const glm::vec3 scale)
    {

    }
};