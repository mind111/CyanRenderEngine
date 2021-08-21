#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "Mesh.h"
#include "Transform.h"

#define kEntityNameMaxLen 64u

// TODO: Components

// entity
struct Entity
{
    char m_name[kEntityNameMaxLen];
    uint32_t m_entityId;
    Transform m_instanceTransform;
    glm::mat4 m_worldTransformMatrix;
    Cyan::MeshInstance* m_meshInstance;
    // flags
    bool m_lit;
    bool m_hasTransform;
    // entity hierarchy
    Entity* m_parent;
    std::vector<Entity*> m_child;
    // scene componet

    glm::vec3 worldPosition()
    {
        glm::vec4 translation = m_worldTransformMatrix[3];
        return glm::vec3(translation.x, translation.y, translation.z);
    }
};