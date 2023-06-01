#pragma once

#include "CyanCore.h"

namespace Cyan
{
    struct VertexBuffer;
    struct IndexBuffer;

    struct VertexArray : public GfxResource
    {
        VertexArray() = delete;
        VertexArray(VertexBuffer* inVertexBuffer, IndexBuffer* inIndexBuffer);
        ~VertexArray();

        static VertexArray* getDummyVertexArray();

        void init();
        void bind();
        void unbind();

    private:
        void bindVertexBuffer(VertexBuffer* inVertexBuffer);
        void bindIndexBuffer(IndexBuffer* inIndexBuffer);

        VertexBuffer* vb = nullptr;
        IndexBuffer* ib = nullptr;
    };
}
