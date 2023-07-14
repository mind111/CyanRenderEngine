#include "Core.h"
#include "GLBuffer.h"

namespace Cyan
{
    GLVertexBuffer::GLVertexBuffer(i32 sizeInBytes, const void* data)
        : GLObject()
    {
        glCreateBuffers(1, &m_name);
        // todo: expose the usage parameter
        glNamedBufferData(m_name, sizeInBytes, data, GL_STATIC_DRAW);
    }
    
    GLVertexBuffer::~GLVertexBuffer() 
    {
        glDeleteBuffers(1, &m_name);
    }

    void GLVertexBuffer::bind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_name);
    }

    void GLVertexBuffer::unbind()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}