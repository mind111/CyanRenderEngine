#pragma once
#include <string>
#include <map>
#include "glew.h"

#include "MathUtils.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "Material.h"

namespace Cyan
{
    struct MeshInstance;

    struct Mesh
    {
        MeshInstance* createInstance();

        struct SubMesh
        {
            VertexArray* m_vertexArray;
            u32 m_numVerts;
        };

        std::string m_name;
        glm::mat4 m_normalization;
        std::vector<SubMesh*> m_subMeshes;
    };

    struct MeshInstance
    {
        void setMaterial(u32 index, MaterialInstance* matl);

        Mesh* m_mesh;
        MaterialInstance** m_matls;
    };
}

extern float cubeVertices[108];