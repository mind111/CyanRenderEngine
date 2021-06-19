#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "Mesh.h"
#include "Transform.h"

// entity
struct Entity
{
    uint32_t m_entityId;
    glm::vec3 m_position;
    Transform* m_xform;
    Cyan::MeshInstance* m_meshInstance;
    bool m_lit;
};