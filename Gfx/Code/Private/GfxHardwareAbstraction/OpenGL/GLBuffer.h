#pragma once

#include "Core.h"
#include "GHBuffer.h"
#include "GLObject.h"
#include "Geometry.h"

namespace Cyan
{
    class GLVertexBuffer : public GLObject, public GHVertexBuffer
    {
    public:
        GLVertexBuffer(const VertexSpec& vertexSpec, i32 sizeInBytes, const void* data);
        virtual ~GLVertexBuffer();

        virtual void bind() override;
        virtual void unbind() override;
        VertexSpec getVertexSpec() { return m_vertexSpec; }
    private:
        VertexSpec m_vertexSpec;
    };

    class GLIndexBuffer : public GLObject, public GHIndexBuffer
    {
    public:
        GLIndexBuffer(const std::vector<u32>& indices);
        ~GLIndexBuffer();

        virtual void bind() override;
        virtual void unbind() override;
    };

    class GLVertexArray : public GLObject
    {
    public:
        GLVertexArray(GLVertexBuffer* vb, GLIndexBuffer* ib);
        ~GLVertexArray();

        void bind();
        void unbind();
    private:
        GLVertexBuffer* m_vb = nullptr;
        GLIndexBuffer* m_ib = nullptr;
    };
}
