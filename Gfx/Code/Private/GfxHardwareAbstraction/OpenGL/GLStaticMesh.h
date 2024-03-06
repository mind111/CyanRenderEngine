#pragma once

#include "Core.h"
#include "GfxStaticMesh.h"

namespace Cyan
{
    class GLVertexBuffer;
    class GLIndexBuffer;
    class GLVertexArray;
    struct Geometry;

    class GLStaticSubMesh : public GfxStaticSubMesh
    {
    public:
        enum class PrimitiveMode
        {
            kTriangles = 0,
            kDefault = 0,
            kLines,
            kPoints,
            kInvalid
        };

        GLStaticSubMesh(Geometry* geometry);
        ~GLStaticSubMesh();
        
        u32 numVertices() { return m_numVertices; }
        u32 numIndices() { return m_numIndices; }

        virtual void updateVertices(u32 byteOffset, u32 numOfBytes, const void* data) override;
        virtual void draw() override;
    private:
        u32 m_numVertices = 0, m_numIndices = 0;
        PrimitiveMode m_primitiveMode = PrimitiveMode::kInvalid;
        std::unique_ptr<GLVertexBuffer> m_vb = nullptr;
        std::unique_ptr<GLIndexBuffer> m_ib = nullptr;
        std::unique_ptr<GLVertexArray> m_va = nullptr;
    };
}