#include "GLShader.h"
#include "GLTexture.h"
#include "glew.h"

namespace Cyan
{
    GLShader::GLShader(const char* text)
        : GLObject(), m_text(text)
    {
    }

    GLShader::~GLShader()
    {
        glDeleteShader(m_name);
    }

    GLShaderUniformDesc::Type translate(GLenum glType)
    {
        using UniformType = GLShaderUniformDesc::Type;

        switch (glType) 
        {
        case GL_SAMPLER_2D:
            return UniformType::kSampler2D;
        case GL_SAMPLER_2D_ARRAY:
            return UniformType::kSampler2DArray;
        case GL_SAMPLER_3D:
            return UniformType::kSampler3D;
        case GL_SAMPLER_2D_SHADOW:
            return UniformType::kSamplerShadow;
        case GL_SAMPLER_CUBE:
            return UniformType::kSamplerCube;
        case GL_IMAGE_3D:
            return UniformType::kImage3D;
        case GL_UNSIGNED_INT_IMAGE_3D:
            return UniformType::kImageUI3D;
        case GL_UNSIGNED_INT:
            return UniformType::kUint;
        case GL_UNSIGNED_INT_ATOMIC_COUNTER:
            return UniformType::kAtomicUint;
        case GL_INT:
            return UniformType::kInt;
        case GL_FLOAT:
            return UniformType::kFloat;
        case GL_FLOAT_MAT4:
            return UniformType::kMat4;
        case GL_FLOAT_VEC3:
            return UniformType::kVec3;
        case GL_FLOAT_VEC4:
            return UniformType::kVec4;
        default:
            return UniformType::kCount;
        }
    }

    void GLShader::build()
    {
        GLuint program = m_name;
        // query list of active uniforms
        i32 activeNumUniforms = -1;
        glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &activeNumUniforms);
        if (activeNumUniforms > 0) 
        {
            i32 maxNameLength = -1;
            glGetProgramInterfaceiv(program, GL_UNIFORM, GL_MAX_NAME_LENGTH, &maxNameLength);
            if (maxNameLength > 0)
            {
                u32 nameLength = maxNameLength + 1;
                char* name = static_cast<char*>(_malloca(nameLength));
                if (name != nullptr)
                {
                    for (i32 i = 0; i < activeNumUniforms; ++i) 
                    {
                        GLenum type; i32 count;
                        glGetActiveUniform(program, i, maxNameLength, nullptr, &count, &type, name);
                        auto translated = translate(type);
                        for (i32 j = 0; j < count; ++j) 
                        {
                            if (count > 1)
                            {
                                char* nextToken = nullptr;
                                char* token = strtok_s(name, "[", &nextToken);
                                char suffix[5] = "[%u]";
                                size_t formatLength = strlen(token) + strlen(suffix) + 1;
                                char* format = static_cast<char*>(_malloca(formatLength));
                                if (format != nullptr)
                                {
                                    strcat_s(format, formatLength, suffix);
                                    sprintf_s(name, nameLength, format, j);
                                }
                            }

                            GLShaderUniformDesc desc = { };
                            desc.type = translated;
                            desc.name = std::string(name);
                            desc.location = glGetUniformLocation(program, name);
                            m_uniformDescMap.insert({ desc.name, desc });
                        }
                    }
                }
            }
        }
    }

    void GLShader::bindTexture(const char* samplerName, GHTexture* texture)
    {
        auto glTexture = dynamic_cast<GLTexture*>(texture);
        if (glTexture != nullptr)
        {
            glTexture->bind();
            i32 textureUnit = glTexture->getBoundTextureUnit();
            glSetUniform(samplerName, textureUnit);
        }
    }

    void GLShader::unbindTexture(const char* samplerName, GHTexture* texture)
    {
        auto glTexture = dynamic_cast<GLTexture*>(texture);
        if (glTexture != nullptr)
        {
            glTexture->unbind();
            glSetUniform(samplerName, -1);
        }
    }

    i32 GLShader::getUniformLocation(const char* name)
    {
        auto entry = m_uniformDescMap.find(std::string(name));
        if (entry == m_uniformDescMap.end()) 
        {
            return -1;
        }
        return entry->second.location;
    }

#define GL_SET_UNIFORM(func, ...)                \
    i32 location = getUniformLocation(name);     \
    if (location >= 0) {                         \
        func(m_name, location, __VA_ARGS__);     \
    }                                            \

    void GLShader::glSetUniform(const char* name, f32 data) 
    {
        GL_SET_UNIFORM(glProgramUniform1f, data)
    }

    void GLShader::glSetUniform(const char* name, u32 data) 
    {
        GL_SET_UNIFORM(glProgramUniform1ui, data)
    }

    void GLShader::glSetUniform(const char* name, i32 data) 
    {
        GL_SET_UNIFORM(glProgramUniform1i, data)
    }

    void GLShader::glSetUniform(const char* name, const glm::vec2& data) 
    {
        GL_SET_UNIFORM(glProgramUniform2f, data.x, data.y)
    }

    void GLShader::glSetUniform(const char* name, const glm::uvec2& data)
    {
        GL_SET_UNIFORM(glProgramUniform2ui, data.x, data.y)
    }

    void GLShader::glSetUniform(const char* name, const glm::vec3& data) 
    {
        GL_SET_UNIFORM(glProgramUniform3f, data.x, data.y, data.z)
    }
    
    void GLShader::glSetUniform(const char* name, const glm::vec4& data) 
    {
        GL_SET_UNIFORM(glProgramUniform4f, data.x, data.y, data.z, data.w)
    }

    void GLShader::glSetUniform(const char* name, const glm::mat4& data) 
    {
        GL_SET_UNIFORM(glProgramUniformMatrix4fv, 1, false, &data[0][0])
    }

    void GLShader::glSetUniform(const char* name, const u64& data) 
    {
        GL_SET_UNIFORM(glProgramUniformHandleui64ARB, data);
    }

    void GLShader::glSetUniform(const char* name, const glm::ivec2& data) 
    {
        GL_SET_UNIFORM(glProgramUniform2i, data.x, data.y);
    }

    GLVertexShader::GLVertexShader(const char* text)
        : GLShader(text)
    {
        const char* strings[1] = { m_text };
        m_name = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, strings);
    }

    GLPixelShader::GLPixelShader(const char* text)
        : GLShader(text)
    {
        const char* strings[1] = { m_text };
        m_name = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, strings);
    }

    GLComputeShader::GLComputeShader(const char* text)
        : GLShader(text)
    {
        const char* strings[1] = { m_text };
        m_name = glCreateShaderProgramv(GL_COMPUTE_SHADER, 1, strings);
    }
 }