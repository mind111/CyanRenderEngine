#include <iostream>
#include <fstream>
#include <sstream>
#include "shader.h"

void checkShaderCompilation(GLuint shader) {
    int compile_result;
    char log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_result);
    if (!compile_result) {
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cout << log << std::endl;
    }
}

void checkShaderLinkage(GLuint shader) {
    int link_result;
    char log[512];
    glGetShaderiv(shader, GL_LINK_STATUS, &link_result);
    if (!link_result) {
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cout << log << std::endl;
    }
}


const char* readShaderFile(const char* filename) {
    // Open file
    std::fstream shader_file(filename);
    std::string src_str;
    const char* vertex_shader_src = nullptr; 
    if (shader_file.is_open()) {
        std::stringstream src_str_stream;
        std::string line;
        while (std::getline(shader_file, line)) {
            src_str_stream << line << "\n";
        }
        src_str = src_str_stream.str();
        // Copy the string over to a char array
        vertex_shader_src = new char[src_str.length()];
        // + 1 here to include null character
        std::memcpy((void*)vertex_shader_src, src_str.c_str(), src_str.length() + 1);
    } else {
        std::cout << "ERROR: Cannot open shader source file!" << std::endl;
    }
    // close file
    shader_file.close();
    return vertex_shader_src;
}

void Shader::init() {
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    shaderProgram = glCreateProgram();
}

void Shader::initUniformLoc(const std::vector<std::string>& uniformNames) {
    for (auto name : uniformNames) {
        GLint location = glGetUniformLocation(shaderProgram, name.c_str());
        uniformLocMap.insert(std::pair<std::string, GLint>(name, location));
    }
}

void Shader::loadShaderSrc(const char* vertFileName, 
                           const char* fragFileName) {
    const char* vertShaderSrc = readShaderFile(vertFileName);
    const char* fragShaderSrc = readShaderFile(fragFileName);
    glShaderSource(vertexShader, 1, &vertShaderSrc, nullptr);
    glShaderSource(fragmentShader, 1, &fragShaderSrc, nullptr);
}

void Shader::generateShaderProgram() {
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    checkShaderCompilation(vertexShader);
    checkShaderCompilation(fragmentShader);
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkShaderLinkage(shaderProgram);
}

void Shader::setUniform1i(const char* uniformName, GLint data) {
    GLint intLocation = uniformLocMap[uniformName];
    glUniform1i(intLocation, data);
}

void Shader::setUniform1f(const char* uniformName, GLfloat data) {
    GLint floatLocation = uniformLocMap[uniformName];
    glUniform1f(floatLocation, data);
}

void Shader::setUniformVec3(const char* uniformName, GLfloat* vecData) {
    GLint vecLocation = uniformLocMap[uniformName];
    glUniform3fv(vecLocation, 1, vecData);
}

void Shader::setUniformMat4f(const char* uniformName, GLfloat* matData) {
    GLint matLocation = uniformLocMap[uniformName];
    glUniformMatrix4fv(matLocation, 1, GL_FALSE, matData);
}

void Shader::bindShader() {
    glUseProgram(shaderProgram);
}
