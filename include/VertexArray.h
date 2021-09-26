#pragma once

#include "VertexBuffer.h"

// TODO: only supports interleaved buffer for now
struct VertexArray
{
    void init();
    u32 numVerts()
    {
        return m_vertexBuffer->m_numVerts;
    }

    VertexBuffer* m_vertexBuffer;
    u32           m_numIndices;
    GLuint        m_ibo;
    GLuint        m_vao;
};