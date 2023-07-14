#pragma once

namespace Cyan
{
    class GHVertexBuffer
    {
    public:
        static GHVertexBuffer* create();

        virtual ~GHVertexBuffer() { }

        virtual void bind() = 0;
        virtual void unbind() = 0;
    };

    class GHIndexBuffer
    {
    public:
        static GHIndexBuffer* create();

        virtual ~GHIndexBuffer() { }

        virtual void bind() = 0;
        virtual void unbind() = 0;
    };
}
