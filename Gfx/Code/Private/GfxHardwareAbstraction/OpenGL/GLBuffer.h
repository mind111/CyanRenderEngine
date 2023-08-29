#pragma once

#include "Core.h"
#include "GfxHardwareAbstraction/GHInterface/GHBuffer.h"
#include "GfxHardwareAbstraction/OpenGL/GLObject.h"
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

        void update(u32 byteOffset, u32 numOfBytes, const void* data);
        VertexSpec getVertexSpec() { return m_vertexSpec; }
    private:
        VertexSpec m_vertexSpec;
        i32 m_bufferSizeInBytes;
    };

    class GLIndexBuffer : public GLObject, public GHIndexBuffer
    {
    public:
        GLIndexBuffer(const std::vector<u32>& indices);
        ~GLIndexBuffer();

        virtual void bind() override;
        virtual void unbind() override;
    private:
        i32 m_bufferSizeInBytes;
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

#define INVALID_SHADER_STORAGE_UNIT -1
    class GLShaderStorageBuffer : public GLObject, public GHRWBuffer
    {
    public:
        GLShaderStorageBuffer(u32 inSizeInBytes);
        virtual ~GLShaderStorageBuffer();

        virtual void bind() override;
        virtual void unbind() override;
        virtual void read(void* dst, u32 dstOffset, u32 srcOffset, u32 bytesToRead) override;
        virtual void write(void* src, u32 srcOffset, u32 dstOffset, u32 bytesToWrite) override;

        bool isBound() { return m_boundUnit > INVALID_SHADER_STORAGE_UNIT; }

    private:
        u32 m_sizeInBytes = 0;
        i32 m_boundUnit = INVALID_SHADER_STORAGE_UNIT;
    };
}
