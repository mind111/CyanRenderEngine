#pragma once
#include <string>
#include <vector>

#include "glew.h"

enum class meshattribtype
{
    position = 0,
    normal,
    textureuv,
    tangents
};

struct VertexAttrib
{
    std::string desc;  // Name of the attrbute
    GLenum type;       // Data type
    int size;          // num of components
    int offset;        // Offset relative to the first vertex attribute
};

class VertexBuffer
{
public:
    VertexBuffer() { }
    ~VertexBuffer() { }
    static VertexBuffer* create(float* data, int size);
    void pushVertexAttribute(VertexAttrib attrib);
    std::vector<VertexAttrib>& getAttribs() { return mVertexAttribs; }
    GLuint getBufferId() { return mBufferId; }
    unsigned int mStride;
private:
    std::vector<VertexAttrib> mVertexAttribs;
    GLuint mBufferId;
};
