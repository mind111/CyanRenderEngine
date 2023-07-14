#pragma once

namespace Cyan
{
    class VertexBuffer
    {
    public:
        static VertexBuffer* create();

        virtual ~VertexBuffer() { }

        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    class IndexBuffer
    {
    public:
        static IndexBuffer* create();

        virtual ~IndexBuffer() { }

        virtual void bind() = 0;
        virtual void unbind() = 0;
    };
}
