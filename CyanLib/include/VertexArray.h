#pragma once

#include "VertexBuffer.h"

// todo: only supports interleaved buffer for now
struct VertexArray
{
    VertexArray(VertexBuffer* srcVb, std::vector<u32>* indices=nullptr)
        : vb(srcVb), ibo(-1), vao(-1)
    {
        init(indices);
    }

    // u32 numVerts() { return vb->m_numVerts; }
    bool hasIndexBuffer() { return ibo != (u32)-1; }
    GLuint getGLObject() { return vao; }
    void release() 
    {
        vb->release();
        delete vb;
        glDeleteBuffers(1, &ibo);
        glDeleteBuffers(1, &vao);
    }

private:
    void init(std::vector<u32>* indices);

    VertexBuffer* vb;
    GLuint        ibo;
    GLuint        vao;
};

namespace Cyan
{
    VertexArray* createVertexArray(VertexBuffer* vb, std::vector<u32>* indices);
}