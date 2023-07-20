#include "Core.h"
#include "GLBuffer.h"

namespace Cyan
{
    GLVertexBuffer::GLVertexBuffer(const VertexSpec& vertexSpec, i32 sizeInBytes, const void* data)
        : GLObject(), m_vertexSpec(vertexSpec)
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

    GLIndexBuffer::GLIndexBuffer(const std::vector<u32>& indices)
    {
        glCreateBuffers(1, &m_name);
        u32 sizeInBytes = sizeof(u32) * indices.size();
        glNamedBufferData(m_name, sizeInBytes, indices.data(), GL_STATIC_DRAW);
    }

    GLIndexBuffer::~GLIndexBuffer()
    {
        glDeleteBuffers(1, &m_name);
    }

    void GLIndexBuffer::bind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_name);
    }

    void GLIndexBuffer::unbind()
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    GLVertexArray::GLVertexArray(GLVertexBuffer* vb, GLIndexBuffer* ib)
        : m_vb(vb), m_ib(ib)
    {
        assert(vb != nullptr && ib != nullptr);
        glCreateVertexArrays(1, &m_name);

        const auto& vertexSpec = vb->getVertexSpec();
        glVertexArrayVertexBuffer(m_name, 0, vb->getName(), 0, vertexSpec.getSizeInBytes());
        for (i32 i = 0; i < vertexSpec.numAttributes(); ++i)
        {
            const auto& attribute = vertexSpec[i];
            GLint size = 0;
            GLenum dataType;
            switch (attribute.getType())
            {
            case VertexAttribute::Type::kPosition: 
            case VertexAttribute::Type::kNormal:
                size = 3;
                dataType = GL_FLOAT;
                break;
            case VertexAttribute::Type::kTangent: 
                size = 4;
                dataType = GL_FLOAT;
                break;
            case VertexAttribute::Type::kTexCoord0: 
            case VertexAttribute::Type::kTexCoord1: 
                size = 2;
                dataType = GL_FLOAT;
                break;
            default: 
                assert(0); 
                break;
            }

            glVertexAttribPointer(i, size, dataType, GL_FALSE, vertexSpec.getSizeInBytes(), (const void*)attribute.getOffset());
            glEnableVertexArrayAttrib(m_name, i);
        }
        glVertexArrayElementBuffer(m_name, ib->getName());
    }

    GLVertexArray::~GLVertexArray()
    {

    }

    void GLVertexArray::bind()
    {
        glBindVertexArray(m_name);
    }

    void GLVertexArray::unbind()
    {
        glBindVertexArray(0);
    }
}