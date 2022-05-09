#pragma once

#include <vector>
#include <queue>

#include "Common.h"

#define kNodeNameMaxLen 64u

struct Node
{
    char m_name[kNodeNameMaxLen];
    Node* m_parent;
    std::vector<Node*> m_child; 

    virtual bool isNamed(const char* name)
    {
        return (strcmp(m_name, name) == 0);
    }

    virtual Node* find(const char* name)
    {
        // breath first search
        std::queue<Node*> nodesQueue;
        nodesQueue.push(this);
        while (!nodesQueue.empty())
        {
            Node* node = nodesQueue.front();
            nodesQueue.pop();
            if (node->isNamed(name))
            {
                return node;
            }
            for (auto* child : node->m_child)
            {
                nodesQueue.push(child);
            }
        }
        return nullptr;
    }
    
    virtual Node* remove(const char* name)
    {
        Node* nodeToRemove = find(name);
        if (nodeToRemove) {
            Node* parent = nodeToRemove->m_parent;
            u32 index = 0;
            for (u32 i = 0; i < parent->m_child.size(); ++i)
            {
                if (parent->m_child[i] == nodeToRemove) 
                {
                    index = i;
                    break;
                }
            }
            nodeToRemove->m_parent = nullptr;
            parent->m_child.erase(parent->m_child.begin() + index);
        }
        return nodeToRemove;
    }

    // attachChild a new child
    virtual void attachChild(Node* child)
    {
        child->m_parent = this;
        m_child.push_back(child);
        child->update();
    }

    virtual void onAttach()
    {

    }

    // detach from current parent
    virtual Node* detach()
    {
        return m_parent->remove(this->m_name);
    }

    virtual void update() = 0;
};