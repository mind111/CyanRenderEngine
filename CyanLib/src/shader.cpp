#include <iostream>
#include <fstream>
#include <sstream>

#include "gtc/type_ptr.hpp"

#include "Shader.h"
#include "Scene.h"
#include "Entity.h"

// todo: how to utilize the following
namespace Cyan
{
    // global shader definitions shared by multiple shaders
    const char* gCommonMathDefs = {
        R"(
            #define PI 3.1415926f
        )"
    };

    const char* gDrawDataDef = {
        R"(
            layout(std430, binding = 0) buffer GlobalDrawData
            {
                mat4  view;
                mat4  projection;
                mat4  sunLightView;
                mat4  sunShadowProjections[4];
                int   numDirLights;
                int   numPointLights;
                float m_ssao;
                float dummy;
            } gDrawData;
        )"
    };

    const char* gGlobalTransformDef = {
        R"(
            layout(std430, binding = 3) buffer InstanceTransformData
            {
                mat4 models[];
            } gInstanceTransforms;
        )"
    };
}

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
        const char* shader_src = nullptr;
        if (shader_file.is_open()) {
            std::stringstream src_str_stream;
            std::string line;
            while (std::getline(shader_file, line)) {
                src_str_stream << line << "\n";
            }
            src_str = src_str_stream.str();
            // Copy the string over to a char array
            shader_src = new char[src_str.length() + 1];
            // + 1 here to include null character
            std::memcpy((void*)shader_src, src_str.c_str(), src_str.length() + 1);
        } else {
            std::cout << "ERROR: Cannot open shader source file!" << std::endl;
        }
        // close file
        shader_file.close();
        return shader_src;
    }
}

Shader::Shader()
{

}

void Shader::buildVsPsFromSource(const char* vertSrc, const char* fragSrc) 
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    handle = glCreateProgram();
    // Load shader source
    const char* vertShaderSrc = ShaderUtil::readShaderFile(vertSrc);
    const char* fragShaderSrc = ShaderUtil::readShaderFile(fragSrc);
    glShaderSource(vs, 1, &vertShaderSrc, nullptr);
    glShaderSource(fs, 1, &fragShaderSrc, nullptr);
    // Compile shader
    glCompileShader(vs);
    glCompileShader(fs);
    ShaderUtil::checkShaderCompilation(vs);
    ShaderUtil::checkShaderCompilation(fs);
    // Link shader
    glAttachShader(handle, vs);
    glAttachShader(handle, fs);
    glLinkProgram(handle);
    ShaderUtil::checkShaderLinkage(handle);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void Shader::buildCsFromSource(const char* csSrcFile)
{
    const char* csSrc = ShaderUtil::readShaderFile(csSrcFile);
    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    handle = glCreateProgram();
    // upload shader source
    glShaderSource(cs, 1, &csSrc, nullptr);
    // Compile shader
    glCompileShader(cs);
    ShaderUtil::checkShaderCompilation(cs);
    // Link shader
    glAttachShader(handle, cs);
    glLinkProgram(handle);
    ShaderUtil::checkShaderLinkage(handle);
    glDeleteShader(cs);
}

void Shader::buildVsGsPsFromSource(const char* vsSrcFile, const char* gsSrcFile, const char* fsSrcFile)
{
    const char* vsSrc = ShaderUtil::readShaderFile(vsSrcFile);
    const char* gsSrc = ShaderUtil::readShaderFile(gsSrcFile);
    const char* fsSrc = ShaderUtil::readShaderFile(fsSrcFile);
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    handle = glCreateProgram();
    // upload shader source
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glShaderSource(gs, 1, &gsSrc, nullptr);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    // Compile shader
    glCompileShader(vs);
    glCompileShader(gs);
    glCompileShader(fs);
    ShaderUtil::checkShaderCompilation(vs);
    ShaderUtil::checkShaderCompilation(gs);
    ShaderUtil::checkShaderCompilation(fs);
    // Link shader
    glAttachShader(handle, vs);
    glAttachShader(handle, gs);
    glAttachShader(handle, fs);
    glLinkProgram(handle);
    ShaderUtil::checkShaderLinkage(handle);
    glDeleteShader(vs);
    glDeleteShader(gs);
    glDeleteShader(fs);
}

void Shader::deleteProgram() {
    glDeleteProgram(handle);
}

void Shader::bind()
{
    // dynamically rebuild the shader if src was modified
    // dynamicRebuild();
    glUseProgram(handle);
}

void Shader::unbind()
{
    glUseProgram(0);
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
    const char* name = _uniform->m_name;
    switch (_uniform->m_type)
    {
        case Uniform::Type::u_float:
            setUniform1f(name, *(float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_int:
            setUniform1i(name, *(int*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_uint:
            setUniform1ui(name, *(u32*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_atomic_uint:
            setUniform1ui(name, *(u32*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_vec3:
            setUniformVec3(name, (float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_vec4:
            setUniformVec4(name, (float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_mat4:
            setUniformMat4(name, (float*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_sampler2D:
            setUniform1i(name, *(int*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_sampler2DArray:
            setUniform1i(name, *(int*)_uniform->m_valuePtr);
            break;
        case Uniform::Type::u_sampler2DShadow:
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
    setUniform1i(_uniform->m_name, _value);
}

void Shader::setUniform(Uniform* _uniform, f32 _value)
{
    setUniform1i(_uniform->m_name, _value);
}

Shader& Shader::setUniform1i(const char* name, GLint data) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform1i, handle, data);
    return *this;
}

Shader& Shader::setUniform1ui(const char* name, GLuint data) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform1ui, handle, data);
    return *this;
}

Shader& Shader::setUniform1f(const char* name, GLfloat data) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform1f, handle, data);
    return *this;
}

Shader& Shader::setUniformVec2(const char* name, const GLfloat* v)
{
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform2fv, handle, 1, v);
    return *this;
}

Shader& Shader::setUniformVec3(const char* name, const GLfloat* vecData) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform3fv, handle, 1, vecData);
    return *this;
}

Shader& Shader::setUniformVec4(const char* name, const GLfloat* vecData) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform4fv, handle, 1, vecData);
    return *this;
}

Shader& Shader::setUniformMat4(const char* name, const GLfloat* matData) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniformMatrix4fv, handle, 1, GL_FALSE, matData);
    return *this;
}

GLint Shader::getUniformLocation(const char* name)
{
    if (m_uniformLocationCache.find(name) != m_uniformLocationCache.end())
    {
        return m_uniformLocationCache[name];
    }
    else 
    {
        GLint location = glGetUniformLocation(handle, name);
        if (location >= 0)
        {
            m_uniformLocationCache[name] = location;
            return location;
        }
    }

    // TODO: Debug message
    // printf("Uniform: %s cannot be found in shader progam: %d\n", name, handle);
    return -1;
}