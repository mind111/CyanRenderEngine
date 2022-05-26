#pragma once
#include <string>
#include <vector>

#include "glew.h"

#include "Common.h"

struct VertexAttribute
{
    enum class Type
    {
        kPosition = 0,
        kNormal,
        kTangent, 
        kTexCoord0,
        kTexCoord1,
        kCount
    };

    std::string name;
    u32 numComponent;
    u32 offset;
};

struct VertexSpec
{
    void addAttribute(VertexAttribute&& attribute)
    {
        attribute.offset = stride;
        stride += attribute.numComponent * sizeof(f32);
        attributes.push_back(attribute);
    }

    u32 getStride() { return stride; }
    u32 getNumAttributes() { return (u32)attributes.size(); }
    const VertexAttribute& getAttribute(u32 index) { return attributes[index]; }

private:
    u32 stride = 0u;
    std::vector<VertexAttribute> attributes;
};

struct VertexBuffer
{
    VertexBuffer(void* data, u32 size, VertexSpec&& srcVertexSpec)
        : vbo(-1), bufferSize(size)
    {
        glCreateBuffers(1, &vbo);
        glNamedBufferData(vbo, size, data, GL_STATIC_DRAW);
        vertexSpec = std::move(srcVertexSpec);
    }

    u32 getStride() { return vertexSpec.getStride(); }
    u32 getNumAttributes() { return vertexSpec.getNumAttributes(); }
    const VertexAttribute& getAttribute(u32 index) { return vertexSpec.getAttribute(index); };
    GLuint getGLObject() { return vbo; }
    void release()
    {
        glDeleteBuffers(1, &vbo);
    }

private:
    VertexSpec vertexSpec;
    u32 bufferSize = 0;
    GLuint vbo = -1;
};