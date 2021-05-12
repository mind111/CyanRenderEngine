#include <iostream>
#include <fstream>
#include <sstream>

#include "gtc/type_ptr.hpp"

#include "Shader.h"
#include "Scene.h"
#include "Entity.h"

namespace ShaderUtil {
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
}

Shader::Shader()
{

}

void Shader::bind()
{
    glUseProgram(m_programId);
}

void Shader::unbind()
{
    glUseProgram(0);
}

void Shader::bindUniform(Uniform* _uniform)
{
    m_uniforms.push_back(_uniform);
}

#define SHADER_GUARD_SET_UNIFORM(name, glFunc, program, ...) \
    if (getUniformLocation(name) >= 0) \
    { \
        GLint loc = m_uniformLocationCache[name]; \
        glFunc(program, loc, __VA_ARGS__); \
    } \
    else \
    { \
      \
    } \

void Shader::setUniform(Uniform* _uniform)
{
    const char* name = _uniform->m_name.c_str();
    switch (_uniform->m_type)
    {
        case Uniform::Type::u_float:
            setUniform1f(name, *(float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_int:
            setUniform1i(name, *(int*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_vec3:
            setUniformVec3(name, (float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_vec4:
            setUniformVec4(name, (float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_mat4:
            setUniformMat4f(name, (float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_sampler2D:
            setUniform1i(name, *(int*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_samplerCube:
            setUniform1i(name, *(int*)_uniform->m_valuePtr);
            break;
        default:
            std::printf("Error when setting uniform %s", name);
            break;
    }
}

void Shader::setUniform(Uniform* _uniform, i32 _value)
{
    setUniform1i(_uniform->m_name.c_str(), _value);
}

void Shader::setUniform(Uniform* _uniform, f32 _value)
{
    setUniform1i(_uniform->m_name.c_str(), _value);
}

void Shader::setUniform1i(const char* name, GLint data) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform1i, m_programId, data);
}

void Shader::setUniform1f(const char* name, GLfloat data) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform1f, m_programId, data);
}

void Shader::setUniformVec3(const char* name, GLfloat* vecData) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform3fv, m_programId, 1, vecData);
}

void Shader::setUniformVec4(const char* name, GLfloat* vecData) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform4fv, m_programId, 1, vecData);
}

void Shader::setUniformMat4f(const char* name, GLfloat* matData) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniformMatrix4fv, m_programId, 1, GL_FALSE, matData);
}

GLint Shader::getUniformLocation(const char* name)
{
    if (m_uniformLocationCache.find(name) != m_uniformLocationCache.end())
    {
        return m_uniformLocationCache[name];
    }
    else 
    {
        GLint location = glGetUniformLocation(m_programId, name);
        if (location >= 0)
        {
            m_uniformLocationCache[name] = location;
            return location;
        }
    }

    // TODO: Debug message
    //std::printf("Uniform: %s cannot be found in shader progam: %d", name, m_programId);
    return -1;
}