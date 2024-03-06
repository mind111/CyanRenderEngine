#pragma once

#include "Gfx.h"
#include "Core.h"

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

    class GFX_API GHRWBuffer
    {
    public:
        static std::unique_ptr<GHRWBuffer> create(u32 sizeInBytes);
        virtual ~GHRWBuffer() { }

        virtual void bind() = 0;
        virtual void unbind() = 0;
        virtual void read(void* dst, u32 dstOffset, u32 srcOffset, u32 bytesToRead) = 0;
        virtual void write(void* src, u32 srcOffset, u32 dstOffset, u32 bytesToWrite) = 0;
    };

    class GFX_API GHAtomicCounterBuffer
    {
    public:
        static std::unique_ptr<GHAtomicCounterBuffer> create();
        virtual ~GHAtomicCounterBuffer() { }

        virtual void bind(u32 bindingUnit) = 0;
        virtual void unbind() = 0;
        virtual void read(u32* dst) = 0;
        virtual void write(u32 data) = 0;
    };
}
