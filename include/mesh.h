#pragma once
#include <string>
#include <map>
#include "glew.h"

struct MeshInstance {
    uint32_t meshID;
    uint32_t instanceID;
};

struct Mesh {
public:
    std::string name;
    std::map<std::string, int> diffuseMapTable;
    std::map<std::string, int> specularMapTable;
    std::string normalMapName, aoMapName, roughnessMapName;
    int normalMapID, aoMapID, roughnessMapID;
    unsigned int numVerts;
    GLuint vao;
    GLuint posVBO;
    GLuint textureCoordVBO;
    GLuint normalVBO;
    GLuint tangentVBO, biTangentVBO;
    GLuint ibo;
};

struct Skybox {
    std::string name;
    GLuint vbo, vao;
    GLuint cubmapTexture;
};

extern float cubeVerts[108];