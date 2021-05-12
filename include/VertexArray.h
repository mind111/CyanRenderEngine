#pragma once

#include "VertexBuffer.h"

// TODO: only supports interleaved buffer for now
struct VertexArray
{
    void init();

    VertexBuffer* m_vertexBuffer;
    GLuint        m_vao;
};