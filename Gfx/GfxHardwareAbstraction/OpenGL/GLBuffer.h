#pragma once

#include "Core.h"
#include "GHBuffer.h"
#include "GLObject.h"

namespace Cyan
{
    class GLVertexBuffer : public GLObject, public GHVertexBuffer
    {
    public:
        GLVertexBuffer(i32 sizeInBytes, const void* data);
        virtual ~GLVertexBuffer();

        virtual void bind() override;
        virtual void unbind() override;
    private:
    };

    class GLIndexBuffer : public GLObject, public GHIndexBuffer
    {
    public:
    };
}
