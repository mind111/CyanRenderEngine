#pragma once
#include <string>
#include <vector>

#include "glew.h"

#include "Common.h"

struct VertexAttrib
{
    enum DataType
    {
        Float,
        Int
    };

    u32 getSize();

    DataType m_type;
    u32 m_size;
    u32 m_strideInBytes;
    u32 m_offset;
};

struct VertexBuffer
{
    void* m_data;
    u32 m_sizeInBytes;
    u32 m_strideInBytes;
    u32 m_numVerts;
    GLuint m_vbo;
    std::vector<VertexAttrib> m_vertexAttribs;

    void addVertexAttrb(const VertexAttrib& attrib)
    {
        m_vertexAttribs.push_back(attrib);
    }
};