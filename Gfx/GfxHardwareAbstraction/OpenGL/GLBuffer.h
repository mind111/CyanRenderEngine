#pragma once

#include "Buffer.h"

namespace Cyan
{
    class GLVertexBuffer : public VertexBuffer
    {
    public:
        GLVertexBuffer();
        virtual ~GLVertexBuffer();
    };

    class GLIndexBuffer : public IndexBuffer
    {
    public:
    };
}
