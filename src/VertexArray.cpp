#include "VertexArray.h"

void VertexArray::init()
{
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer->m_vbo);
    for (u32 index = 0; index < m_vertexBuffer->m_vertexAttribs.size(); index++)
    {
        VertexAttrib& attrib = m_vertexBuffer->m_vertexAttribs[index];
        GLenum glType;
        switch (attrib.m_type)
        {
            case VertexAttrib::DataType::Float:
                glType = GL_FLOAT;
                break;
            case VertexAttrib::DataType::Int:
                glType = GL_INT;
                break;
            default:
                break;
        }
        glEnableVertexArrayAttrib(m_vao, index);
        glVertexAttribPointer(index, attrib.m_size, glType, false, attrib.m_strideInBytes, reinterpret_cast<void*>(attrib.m_offset));
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}