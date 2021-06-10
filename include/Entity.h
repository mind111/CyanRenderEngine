#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "Mesh.h"
#include "Transform.h"

// entity
struct Entity
{
    void setMaterial(u32 subMeshIdx, Cyan::MaterialInstance* matl)
    {
        m_matls[subMeshIdx] = matl;
    }

    uint32_t m_entityId;
    glm::vec3 m_position;
    Transform m_xform;
    Cyan::Mesh* m_mesh;
    Cyan::MaterialInstance** m_matls;
};