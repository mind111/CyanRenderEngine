#pragma once

#include "Windows.h"

#include <vector>
#include <map>

#include "glew.h"

#include "Uniform.h"

namespace ShaderUtil
{
    void checkShaderCompilation(GLuint shader);
    void checkShaderLinkage(GLuint shader);
    const char* readShaderFile(const char* filename);
}

// Shader storage buffer
struct RegularBuffer
{
    const char* m_name;
    const void* m_data;
    u32         m_sizeInBytes;
    GLuint m_binding;
    GLuint m_ssbo;            // Shader storage buffer object
};

class Shader
{
public:
    Shader();
    ~Shader() {}
    
    void bind();
    void unbind();

    void bindUniform(Uniform* _uniform);

    void setUniform(Uniform* _uniform);
    void setUniform(Uniform* _uniform, i32 _value);
    void setUniform(Uniform* _uniform, f32 _value);
    void setUniform1i(const char* name, GLint data);
    void setUniform1f(const char* name, GLfloat data);
    void setUniformVec3(const char* name, GLfloat* vecData);
    void setUniformVec4(const char* name, GLfloat* vecData);
    void setUniformMat4f(const char* name, GLfloat* matData);

    GLint getUniformLocation(const char* _name);

    std::map<const char*, int> m_uniformLocationCache;
    std::vector<Uniform*> m_uniforms;
    std::vector<RegularBuffer*> m_buffers;

    GLuint m_programId;
};