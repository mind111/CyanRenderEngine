#pragma once

#include "CyanCore.h"

namespace Cyan
{
    struct VertexBuffer;
    struct IndexBuffer;

    struct VertexArray : public GpuResource
    {
        VertexArray() = delete;
        VertexArray(VertexBuffer* inVertexBuffer, IndexBuffer* inIndexBuffer);
        ~VertexArray();

        void init();

    private:
        void bindVertexBuffer(VertexBuffer* inVertexBuffer);
        void bindIndexBuffer(IndexBuffer* inIndexBuffer);

        VertexBuffer* vb = nullptr;
        IndexBuffer* ib = nullptr;
    };
}
