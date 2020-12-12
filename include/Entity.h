#pragma once

// External dependencies
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

// Internal dependencies
#include "Mesh.h"
#include "Material.h"
#include "Transform.h"

// Instance of each
struct Entity
{
    uint32_t entityId;
    glm::vec3 position;
    Transform xform;
    MeshGroup* mesh;
    Material* matl;
};

class EntityManager
{

};