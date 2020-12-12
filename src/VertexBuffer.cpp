#include "VertexBuffer.h"

VertexBuffer* VertexBuffer::create(float* data, int size)
{
    VertexBuffer* vb = new VertexBuffer();
    vb->mStride = 0;
    glCreateBuffers(1, &vb->mBufferId);
    glNamedBufferData(vb->mBufferId, size, data, GL_STATIC_DRAW);
    return vb;
}

void VertexBuffer::pushVertexAttribute(VertexAttrib attrib)
{
    attrib.offset = mStride;
    mStride += attrib.size * sizeof(attrib.type);
    mVertexAttribs.push_back(attrib);
}