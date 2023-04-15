#include <cassert>

#include "glm/glm.hpp"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

namespace Cyan
{
    VertexArray::VertexArray(VertexBuffer* inVertexBuffer, IndexBuffer* inIndexBuffer)
        : vb(inVertexBuffer), ib(inIndexBuffer)
    {
        glCreateVertexArrays(1, &glObject);
    }

    VertexArray::~VertexArray()
    {
        GLuint arrays[] = { getGpuResource() };
        glDeleteVertexArrays(1, arrays);
    }

    VertexArray* VertexArray::getDummyVertexArray()
    {
        static VertexBuffer* dummyVertexBuffer = nullptr;
        static IndexBuffer* dummyIndexBuffer = nullptr;
        static VertexArray* dummyVertexArray = nullptr;

        if (dummyVertexArray == nullptr)
        {
            assert(dummyVertexBuffer == nullptr && dummyIndexBuffer == nullptr);
            VertexBuffer::Spec spec;
            spec.addVertexAttribute("Position", VertexBuffer::Attribute::Type::kVec3);
            std::vector<glm::vec3> vertices = { glm::vec3(0.f) };
            u32 sizeInBytes = sizeOfVector(vertices);
            dummyVertexBuffer = new VertexBuffer(spec, vertices.data(), sizeInBytes);
            std::vector<u32> indices = { 0 };
            dummyIndexBuffer = new IndexBuffer(indices);
            dummyVertexArray = new VertexArray(dummyVertexBuffer, dummyIndexBuffer);
        }
        return dummyVertexArray;
    }

    void VertexArray::init()
    {
        assert(vb);
        assert(ib);

        bindVertexBuffer(vb);
        bindIndexBuffer(ib);
    }

    void VertexArray::bindVertexBuffer(VertexBuffer* inVertexBuffer) 
    {
        const auto& spec = inVertexBuffer->spec;

        // only support one vertex buffer for a vertex array for now
        const u32 vertexBufferBinding = 0u;
        glVertexArrayVertexBuffer(getGpuResource(), 0, inVertexBuffer->getGpuResource(), vertexBufferBinding, spec.stride);

        u32 attribIndex = 0;
        for (const auto& attrib : spec.attributes)
        {
            glEnableVertexArrayAttrib(getGpuResource(), attribIndex);

            GLint size; GLenum type;
            switch (attrib.type)
            {
            case VertexBuffer::Attribute::Type::kVec2:
                size = 2;
                type = GL_FLOAT;
                break;
            case VertexBuffer::Attribute::Type::kVec3:
                size = 3;
                type = GL_FLOAT;
                break;
            case VertexBuffer::Attribute::Type::kVec4:
                size = 4;
                type = GL_FLOAT;
                break;
            default:
                assert(0);
            }

            glVertexArrayAttribFormat(getGpuResource(), attribIndex, size, type, GL_FALSE, attrib.offset);
            glVertexArrayAttribBinding(getGpuResource(), attribIndex, vertexBufferBinding);

            attribIndex++;
        }
    }

    void VertexArray::bindIndexBuffer(IndexBuffer* inIndexBuffer) 
    {
        glVertexArrayElementBuffer(getGpuResource(), inIndexBuffer->getGpuResource());
    }

    void VertexArray::bind()
    {
        glBindVertexArray(getGpuResource());
    }

    void VertexArray::unbind()
    {
        glBindVertexArray(0);
    }
}
