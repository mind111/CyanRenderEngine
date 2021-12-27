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

void Shader::getFileWriteTime(ShaderFileInfo& fileInfo) {
    HANDLE fileHandle = CreateFileA(fileInfo.m_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_READONLY,
                                    nullptr);
    GetFileTime(fileHandle, nullptr, nullptr, &fileInfo.m_lastWriteTime);
    CloseHandle(fileHandle);
}

bool Shader::fileHasChanged(ShaderFileInfo& fileInfo) {
    HANDLE fileHandle = CreateFileA(fileInfo.m_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_ALWAYS,
                                    FILE_ATTRIBUTE_READONLY,
                                    nullptr);
    FILETIME lastWriteTime = { };
    GetFileTime(fileHandle, nullptr, nullptr, &lastWriteTime);
    // check if shader source file is modified, need to rebuild shader
    bool wasModified = (CompareFileTime(reinterpret_cast<const LPFILETIME>(&fileInfo.m_lastWriteTime), 
                                        reinterpret_cast<const LPFILETIME>(&lastWriteTime)) == -1);
    CloseHandle(fileHandle);
    return wasModified;
}

void Shader::buildVsPsFromSource(const char* vertSrc, const char* fragSrc) 
{
    if (m_vertSrcInfo.m_path.empty() || m_fragSrcInfo.m_path.empty()) 
    {
        m_vertSrcInfo.m_path = std::string(vertSrc);
        m_fragSrcInfo.m_path = std::string(fragSrc);
    }
    getFileWriteTime(m_vertSrcInfo);
    getFileWriteTime(m_fragSrcInfo);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    m_programId = glCreateProgram();
    // Load shader source
    const char* vertShaderSrc = ShaderUtil::readShaderFile(m_vertSrcInfo.m_path.c_str());
    const char* fragShaderSrc = ShaderUtil::readShaderFile(m_fragSrcInfo.m_path.c_str());
    glShaderSource(vs, 1, &vertShaderSrc, nullptr);
    glShaderSource(fs, 1, &fragShaderSrc, nullptr);
    // Compile shader
    glCompileShader(vs);
    glCompileShader(fs);
    ShaderUtil::checkShaderCompilation(vs);
    ShaderUtil::checkShaderCompilation(fs);
    // Link shader
    glAttachShader(m_programId, vs);
    glAttachShader(m_programId, fs);
    glLinkProgram(m_programId);
    ShaderUtil::checkShaderLinkage(m_programId);
    glDeleteShader(vs);
    glDeleteShader(fs);
}

void Shader::buildCsFromSource(const char* csSrcFile)
{
    const char* csSrc = ShaderUtil::readShaderFile(csSrcFile);
    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    m_programId = glCreateProgram();
    // upload shader source
    glShaderSource(cs, 1, &csSrc, nullptr);
    // Compile shader
    glCompileShader(cs);
    ShaderUtil::checkShaderCompilation(cs);
    // Link shader
    glAttachShader(m_programId, cs);
    glLinkProgram(m_programId);
    ShaderUtil::checkShaderLinkage(m_programId);
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
    m_programId = glCreateProgram();
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
    glAttachShader(m_programId, vs);
    glAttachShader(m_programId, gs);
    glAttachShader(m_programId, fs);
    glLinkProgram(m_programId);
    ShaderUtil::checkShaderLinkage(m_programId);
    glDeleteShader(vs);
    glDeleteShader(gs);
    glDeleteShader(fs);
}

void Shader::dynamicRebuild() {
    bool rebuild = false;
    rebuild |= fileHasChanged(m_vertSrcInfo);
    rebuild |= fileHasChanged(m_fragSrcInfo);
    if (rebuild) 
    {
        deleteProgram();
        buildVsPsFromSource(m_vertSrcInfo.m_path.c_str(), m_fragSrcInfo.m_path.c_str());
    }
}

void Shader::deleteProgram() {
    glDeleteProgram(m_programId);
}

void Shader::bind()
{
    // dynamically rebuild the shader if src was modified
    dynamicRebuild();
    glUseProgram(m_programId);
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
            setUniformMat4f(name, (float*)_uniform->m_valuePtr);
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

void Shader::setUniform1i(const char* name, GLint data) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform1i, m_programId, data);
}

void Shader::setUniform1ui(const char* name, GLuint data) {
    SHADER_GUARD_SET_UNIFORM(name, glProgramUniform1ui, m_programId, data);
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
    // printf("Uniform: %s cannot be found in shader progam: %d\n", name, m_programId);
    return -1;
}