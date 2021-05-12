#include "Common.h"
#include "Mesh.h"

namespace Cyan
{
    void Mesh::setMaterial(u32 _idx, Material* _matl)
    {
        m_subMeshes[_idx]->m_matl = _matl;
    }
};

float cubeVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

#if 0
Mesh::Mesh()
    : m_numVerts(0)
{
    glCreateVertexArrays(1, &m_vao);
}

void Mesh::initVertexAttributes()
{
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer->getBufferId());
    std::vector<VertexAttrib>& attribs = m_vertexBuffer->getAttribs();
    for (int index = 0; index < attribs.size(); index++)
    {
        glEnableVertexAttribArray(index);
        glVertexAttribPointer(index, attribs[index].size, attribs[index].type, false,
                              m_vertexBuffer->mStride, (void*)(attribs[index].offset));
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
#endif 
