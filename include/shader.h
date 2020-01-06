#pragma once

#include <vector>
#include <map>
#include "glew.h"

class Shader {
public:
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint shaderProgram;

    std::map<std::string, GLint> uniformLocMap;

    void init();
    void initUniformLoc(const std::vector<std::string>& uniformNames);
    void loadShaderSrc(const char* vertFileName, const char* fragFileName);
    void generateShaderProgram();
    void bindShader(); 
    void unbindShader();
    
    void setUniform1i(const char* uniformName, GLint data);
    void setUniform1f(const char* uniformName, GLfloat data);
    void setUniformVec3(const char* uniformName, GLfloat* vecData);
    void setUniformMat4f(const char* uniformName, GLfloat* matData);
    
    Shader() {}
    ~Shader() {}
};