#include "VertexArray.h"

void VertexArray::init(std::vector<u32>* indices)
{
    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // bind vertex buffers to this vao and initialize all vertex attributes
    glBindBuffer(GL_ARRAY_BUFFER, vb->getGLObject());
    for (u32 index = 0; index < vb->getNumAttributes(); index++)
    {
        const VertexAttribute& attribute = vb->getAttribute(index);
        glEnableVertexArrayAttrib(vao, index);
        glVertexAttribPointer(index, attribute.numComponent, GL_FLOAT, false, vb->getStride(), reinterpret_cast<void*>(attribute.offset));
    }

    // initialize index buffer
    if (indices)
    {
        glCreateBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glNamedBufferData(ibo, sizeof(u32) * indices->size(), indices->data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

namespace Cyan
{
    VertexArray* createVertexArray(VertexBuffer* vb, std::vector<u32>* indices)
    {
        return new VertexArray(vb, indices);
    }
}
