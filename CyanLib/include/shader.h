#pragma once

#include "Windows.h"

#include <vector>
#include <map>

#include "glew.h"

#include "Uniform.h"
#include "Texture.h"

#define SHADER_SOURCE_PATH "../../shader/"

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

struct ShaderSource
{
    const char* vsSrc; // vertex shader
    const char* fsSrc; // fragment shader
    const char* gsSrc; // geometry shader
    const char* csSrc; // compute shader
};

// todo: implement this!
struct ShaderMetaData
{
    void init(struct Shader* shader) { }

    struct UniformMetaData
    {
        enum class Type
        {
            kInt = 0,
            kUint,
            kFloat,
            kVec2,
            kVec3,
            kVec4,
            kMat4,
            kSampler2D,
            kSampler3D
        };

        i32 location = -1;
        i32 elementCount = 1;
    };

    std::map<std::string, UniformMetaData> uniformMap;
};

class Shader
{
public:
    enum Type
    {
        VsPs = 0,
        VsGsPs,
        Cs,
        Invalid
    };

    Shader();
    ~Shader() {}

    void init(ShaderSource shaderSrc, Type shaderType)
    {
        switch (shaderType)
        {
            case VsPs:
            {
                buildVsPsFromSource(shaderSrc.vsSrc, shaderSrc.fsSrc);
            } break;
            case VsGsPs:
            {
                buildVsGsPsFromSource(shaderSrc.vsSrc, shaderSrc.gsSrc, shaderSrc.fsSrc);
            } break;
            case Cs:
            {
                buildCsFromSource(shaderSrc.csSrc);
            } break;
        }
    }

    void bind();
    void unbind();
    void deleteProgram();

    void setUniform(Uniform* _uniform);
    void setUniform(Uniform* _uniform, i32 _value);
    void setUniform(Uniform* _uniform, f32 _value);

    void setUniform(const char* name, f32 value) { }
    void setUniform(const char* name, i32 value) { }
    void setUniform(const char* name, u32 value) { }
    void setUniform(const char* name, f32* value) { }

    Shader& setTexture(const char* samplerName, Cyan::Texture* texture) { return *this; }
    Shader& setUniform1i(const char* name, GLint data);
    Shader& setUniform1ui(const char* name, GLuint data);
    Shader& setUniform1f(const char* name, GLfloat data);
    Shader& setUniformVec2(const char* name, const GLfloat* data);
    Shader& setUniformVec3(const char* name, const GLfloat* vecData);
    Shader& setUniformVec4(const char* name, const GLfloat* vecData);
    Shader& setUniformMat4(const char* name, const GLfloat* matData);

    GLint getUniformLocation(const char* _name);

    void buildVsPsFromSource(const char* vertSrc, const char* fragSrc);
    void buildCsFromSource(const char* csSrcFile);
    void buildVsGsPsFromSource(const char* vsSrcFile, const char* gsSrcFile, const char* fsSrcFile);

    void dynamicRebuild();
    void getFileWriteTime(ShaderFileInfo& fileInfo);
    bool fileHasChanged(ShaderFileInfo& fileInfo);

    std::map<const char*, int> m_uniformLocationCache;

    std::string m_name;
    ShaderFileInfo m_vertSrcInfo;
    ShaderFileInfo m_fragSrcInfo;
    GLuint handle;
};
