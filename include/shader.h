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
    const void* m_data;
    u32         m_totalSize;
    u32         m_sizeToUpload;
    GLuint m_binding;
    GLuint m_ssbo;
};

struct ShaderFileInfo
{
    std::string m_path;
    FILETIME m_lastWriteTime;
};

class Shader
{
public:
    Shader();
    ~Shader() {}
    
    void bind();
    void unbind();
    void deleteProgram();


    void setUniform(Uniform* _uniform);
    void setUniform(Uniform* _uniform, i32 _value);
    void setUniform(Uniform* _uniform, f32 _value);
    void setUniform1i(const char* name, GLint data);
    void setUniform1f(const char* name, GLfloat data);
    void setUniformVec3(const char* name, GLfloat* vecData);
    void setUniformVec4(const char* name, GLfloat* vecData);
    void setUniformMat4f(const char* name, GLfloat* matData);

    GLint getUniformLocation(const char* _name);

    void buildFromSource(const char* vertSrc, const char* fragSrc);
    void dynamicRebuild();
    void getFileWriteTime(ShaderFileInfo& fileInfo);
    bool fileHasChanged(ShaderFileInfo& fileInfo);

    std::map<const char*, int> m_uniformLocationCache;

    std::string m_name;
    ShaderFileInfo m_vertSrcInfo;
    ShaderFileInfo m_fragSrcInfo;
    GLuint m_programId;
};

struct ShaderDef
{
    enum Type
    {
        VSFS = 0,
        CS,
        Undefined
    };

    std::string m_name;
    std::string m_cs;
    std::string m_vs;
    std::string m_fs;
};