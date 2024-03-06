#include "GLShader.h"
#include "GLTexture.h"
#include "glew/glew.h"
#include "GLBuffer.h"

namespace Cyan
{
    GLShader::GLShader(const std::string& text)
        : GLObject(), m_text(text)
    {
    }

    GLShader::~GLShader()
    {
        glDeleteShader(m_name);
    }

    ShaderUniformDesc::Type translate(GLenum glType)
    {
        using UniformType = ShaderUniformDesc::Type;

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

    void GLShader::build(ShaderUniformMap& outUniformMap)
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
                        auto translatedType = translate(type);
                        for (i32 j = 0; j < count; ++j) 
                        {
                            if (count > 1)
                            {
                                char* nextToken = nullptr;
                                char* token = strtok_s(name, "[", &nextToken);
                                char suffix[5] = "[%u]";
                                size_t formatLength = strlen(token) + strlen(suffix) + 1;
                                char* format = static_cast<char*>(_malloca(formatLength));
                                sprintf_s(format, formatLength, "%s", token);
                                if (format != nullptr)
                                {
                                    strcat_s(format, formatLength, suffix);
                                    sprintf_s(name, nameLength, format, j);
                                }
                            }

                            ShaderUniformDesc desc = { };
                            desc.type = translatedType;
                            desc.name = std::string(name);
                            // desc.location = glGetUniformLocation(program, name);
                            outUniformMap.insert({ desc.name, desc });
                            i32 uniformLocation = glGetUniformLocation(program, name);
                            if (uniformLocation >= 0)
                            {
                                m_uniformLocationMap.insert({ desc.name, uniformLocation });
                            }
                            if (translatedType == ShaderUniformDesc::Type::kAtomicUint)
                            {
                                GLenum props = { GL_ATOMIC_COUNTER_BUFFER_INDEX };
                                GLsizei length = 1;
                                GLint counterIndex = -1;
                                glGetProgramResourceiv(program,
                                    GL_UNIFORM,
                                    i,
                                    1,
                                    &props,
                                    sizeof(counterIndex),
                                    &length,
                                    &counterIndex);

                                AtomicCounterBinding binding = { };
                                binding.name = desc.name;
                                binding.index = counterIndex;
                                m_atomicCounterBindingMap.insert({ desc.name, binding });
                            }
                        }
                    }
                }
            }
        }

        // query list of active shader storage blocks
        i32 activeNumShaderStorageBlock = -1;
        glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &activeNumShaderStorageBlock);
        if (activeNumShaderStorageBlock > 0) 
        {
            i32 maxNameLength = -1;
            glGetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_MAX_NAME_LENGTH, &maxNameLength);
            if (maxNameLength > 0)
            {
                char* name = static_cast<char*>(alloca(maxNameLength + 1));
                for (i32 i = 0; i < activeNumShaderStorageBlock; ++i) 
                {
                    i32 nameLength = -1;
                    glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, i, maxNameLength, &nameLength, name);
                    std::string blockName(name);
                    ShaderStorageBinding binding = { blockName, i, nullptr, -1 };
                    m_shaderStorageBindingMap.insert({ blockName, binding });
                }
            }
        }

        // query list of active atomic counters
        for (auto& entry : m_atomicCounterBindingMap)
        {
            AtomicCounterBinding& binding = entry.second;
            i32 atomicCounterIndex = binding.index;
            GLint bindingUnit = -1;
            glGetActiveAtomicCounterBufferiv(program, atomicCounterIndex, GL_ATOMIC_COUNTER_BUFFER_BINDING, &bindingUnit);
            assert(bindingUnit >= 0);
            binding.bindingUnit = bindingUnit;
        }
    }

    void GLShader::bindTexture(const char* samplerName, GHTexture* texture, bool& outBound)
    {
        auto glTexture = dynamic_cast<GLTexture*>(texture);
        if (glTexture != nullptr)
        {
            if (getUniformLocation(samplerName) >= 0)
            {
                glTexture->bindAsTexture();
                i32 textureUnit = glTexture->getBoundTextureUnit();
                glSetUniform(samplerName, textureUnit);
                outBound = true;
            }
            else
            {
                outBound = false;
            }
        }
    }

    void GLShader::unbindTexture(const char* samplerName, GHTexture* texture)
    {
        auto glTexture = dynamic_cast<GLTexture*>(texture);
        if (glTexture != nullptr)
        {
            glTexture->unbindAsTexture();
            glSetUniform(samplerName, 0);
        }
    }

    void GLShader::bindRWBuffer(const char* bufferName, GHRWBuffer* RWBuffer, bool& outBound)
    {
        auto glShaderStorageBuffer = dynamic_cast<GLShaderStorageBuffer*>(RWBuffer);
        if (glShaderStorageBuffer != nullptr)
        {
            auto entry = m_shaderStorageBindingMap.find(bufferName);
            if (entry != m_shaderStorageBindingMap.end())
            {
                ShaderStorageBinding& binding = entry->second;
                glShaderStorageBuffer->bind();
                assert(glShaderStorageBuffer->isBound());
                GLuint boundUnit = glShaderStorageBuffer->getBoundUnit();
                binding.buffer = glShaderStorageBuffer;
                binding.bufferUnit = boundUnit;
                glShaderStorageBlockBinding(getName(), binding.blockIndex, boundUnit);
                outBound = true;
            }
            else
            {
                outBound = false;
            }
        }
    }

    void GLShader::unbindRWBuffer(const char* bufferName)
    {
        auto entry = m_shaderStorageBindingMap.find(bufferName);
        if (entry != m_shaderStorageBindingMap.end())
        {
            ShaderStorageBinding& binding = entry->second;
            assert(binding.buffer != nullptr);
            binding.buffer->unbind();
            binding.buffer = nullptr;
            binding.bufferUnit = -1;
        }
        else
        {
            assert(0);
        }
    }

    void GLShader::bindAtomicCounter(const char* name, GHAtomicCounterBuffer* counter, bool& bOutBound)
    {
        auto entry = m_atomicCounterBindingMap.find(name);
        if (entry != m_atomicCounterBindingMap.end())
        {
            AtomicCounterBinding& binding = entry->second;
            assert(binding.bindingUnit >= 0);
            counter->bind(binding.bindingUnit);
            binding.counter = counter;
            bOutBound = true;
        }
        else
        {
            bOutBound = false;
        }
    }

    void GLShader::unbindAtomicCounter(const char* name)
    {
        auto entry = m_atomicCounterBindingMap.find(name);
        if (entry != m_atomicCounterBindingMap.end())
        {
            AtomicCounterBinding& binding = entry->second;
            assert(binding.bindingUnit >= 0);
            assert(binding.counter != nullptr);
            binding.counter->unbind();
            binding.counter = nullptr;
        }
        else
        {
            assert(0);
        }
    }

    void GLShader::bindRWTexture(const char* imageName, GHTexture* RWTexture, u32 mipLevel, bool& outBound)
    {
        auto glTexture = dynamic_cast<GLTexture*>(RWTexture);
        if (glTexture != nullptr)
        {
            if (getUniformLocation(imageName) >= 0)
            {
                RWTexture->bindAsRWTexture(mipLevel);
                i32 imageUnit = glTexture->getBoundImageUnit();
                glSetUniform(imageName, imageUnit);
                outBound = true;
            }
            else
            {
                outBound = false;
            }
        }
    }

#define GL_SET_UNIFORM(func, ...)                \
    i32 location = getUniformLocation(name);     \
    if (location >= 0) {                         \
        func(m_name, location, __VA_ARGS__);     \
    }                                            \

    i32 GLShader::getUniformLocation(const char* name)
    {
        auto entry = m_uniformLocationMap.find(std::string(name));
        if (entry == m_uniformLocationMap.end()) 
        {
            return -1;
        }
        return entry->second;
    }

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

    void GLShader::glSetUniform(const char* name, const glm::ivec3& data)
    {
        GL_SET_UNIFORM(glProgramUniform3i, data.x, data.y, data.z);
    }

    static void checkShaderCompilation(const char* strings[], GLenum shaderType)
    {
        // for debugging shader compilation error purpose
        GLuint shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, strings, 0);
        glCompileShader(shader);
        i32 compilationResult = GL_TRUE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compilationResult);
        if (compilationResult != GL_TRUE)
        {
            std::string log;
            i32 infoLogLength = -1;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
            if (infoLogLength > 0) 
            {
                i32 bufferSize = sizeof(char) * infoLogLength;
                char* infoLog = static_cast<char*>(alloca(bufferSize));
                glGetShaderInfoLog(shader, bufferSize, &infoLogLength, infoLog);
                log.assign(infoLog);
            }

            cyanError("%s", log.c_str());
            // assert on the spot to help catching compilation errors
            assert(0);
        }
        glDeleteShader(shader);
    }

    static void checkProgramLinkage(GLuint program)
    {
        std::string log;
        i32 result = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &result);
        if (result == GL_FALSE) 
        {
            i32 infoLogLength = -1;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
            if (infoLogLength > 0) 
            {
                i32 bufferSize = sizeof(char) * infoLogLength;
                char* infoLog = static_cast<char*>(alloca(bufferSize));
                glGetProgramInfoLog(program, bufferSize, &infoLogLength, infoLog);
                log.assign(infoLog);

                cyanError("%s", log.c_str());
                // assert on the spot to help catching linking errors
                assert(0);
            }
        }
    }

    GLVertexShader::GLVertexShader(const std::string& text)
        : GLShader(text)
    {
        const char* strings[1] = { m_text.c_str() };
        checkShaderCompilation(strings, GL_VERTEX_SHADER);

        m_name = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, strings);
        checkProgramLinkage(m_name);

        build(m_uniformMap);
    }

    GLPixelShader::GLPixelShader(const std::string& text)
        : GLShader(text)
    {
        const char* strings[1] = { m_text.c_str() };
        checkShaderCompilation(strings, GL_FRAGMENT_SHADER);

        m_name = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, strings);
        checkProgramLinkage(m_name);

        build(m_uniformMap);
    }

    GLComputeShader::GLComputeShader(const std::string& text)
        : GLShader(text)
    {
        const char* strings[1] = { m_text.c_str() };
        checkShaderCompilation(strings, GL_COMPUTE_SHADER);

        m_name = glCreateShaderProgramv(GL_COMPUTE_SHADER, 1, strings);
        checkProgramLinkage(m_name);

        build(m_uniformMap);
    }

    GLGeometryShader::GLGeometryShader(const std::string& text)
        : GLShader(text)
    {
        const char* strings[1] = { m_text.c_str() };
        checkShaderCompilation(strings, GL_GEOMETRY_SHADER);

        m_name = glCreateShaderProgramv(GL_GEOMETRY_SHADER, 1, strings);
        checkProgramLinkage(m_name);

        build(m_uniformMap);
    }
 }