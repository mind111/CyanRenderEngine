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
    struct Mesh
    {
        struct SubMesh
        {
            VertexArray* m_vertexArray;
            u32 m_numVerts;
//            MaterialInstance* m_matl;
        };

        std::string m_name;
        glm::mat4 m_normalization;
        std::vector<SubMesh*> m_subMeshes;

        // void setMaterial(u32 _idx, MaterialInstance* _matl);
    };
}

extern float cubeVertices[108];