#pragma once
#include <string>
#include <map>
#include "glew.h"

#include "MathUtils.h"
#include "VertexBuffer.h"

class Mesh;

struct VertexInfo
{
    float* vertexData;
    uint32_t vertexSize;
};

struct MeshGroup
{
    std::vector<Mesh*> subMeshes;
    std::string desc;
    glm::mat4 normalizeXform;
};

// TODO: Make Mesh owns all the related vertex buffer objects
class Mesh 
{
public:
    Mesh();
    void initVertexAttributes();
    unsigned int getNumVerts() { return mNumVerts; };
    void setNumVerts(int vertCount) { mNumVerts = vertCount; }
    VertexBuffer* getVertexBuffer() { return mVertexBuffer; }
    void setVertexBuffer(VertexBuffer* vb) { mVertexBuffer = vb; }
    GLuint getVertexArray() { return mVao; };

    uint8_t activeAttrib;
    uint8_t mAttribFlag;

    unsigned int mNumVerts;
    unsigned int mIbo;
    unsigned int mVao;
    VertexInfo vertexInfo;

private:
    unsigned int computeVertexSize();
private:
    // All attributes are packed in one vertex buffer for now
    VertexBuffer* mVertexBuffer;
};

class MeshManager
{
public:
    static void pushSubMesh(MeshGroup* meshGroup, Mesh* mesh);
};

extern float cubeVertices[108];