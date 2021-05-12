#pragma once

// External dependencies
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

// Internal dependencies
#include "Mesh.h"
#include "Transform.h"

// Instance of each
struct Entity
{
    uint32_t m_entityId;
    glm::vec3 m_position;
    Transform m_xform;
    Cyan::Mesh* m_mesh;
};

class EntityManager
{

};