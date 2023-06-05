#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>

#include <glm/gtc/type_ptr.hpp>

#include "CyanAPI.h"
#include "GfxContext.h"
#include "Shader.h"
#include "Scene.h"
#include "Entity.h"

namespace Cyan
{    
    ShaderSource::ShaderSource(const char* shaderFilePath) 
    {
        // open & load the shader source file into @src
        std::ifstream shaderFile(shaderFilePath);
        if (shaderFile.is_open()) 
        { 
            std::string line;
            bool bParsingHeaders = false;
            while (std::getline(shaderFile, line)) 
            {
                src += line;
                src += '\n';
            }
            shaderFile.close();
        }
        else 
        {
            cyanError("Failed to open shader file %s", shaderFilePath);
        }
    }

    Shader::UniformDesc::Type translate(GLenum glType)
    {
        using UniformType = Shader::UniformDesc::Type;

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

    /** Note - @min:
    * Since the shader class encapsulate a seperable shader program in opengl, try to be more clear
    * about this function is querying a gl program's info log not a gl shader's info log
    */
    bool Shader::getProgramInfoLog(Shader* shader, ShaderInfoLogType type, std::string& output) 
    {
        i32 result = GL_TRUE;
        switch (type) 
        {
        case ShaderInfoLogType::kCompile:
            // querying a gl program's compile status is considered as an invalid operation
        case ShaderInfoLogType::kLink:
            glGetProgramiv(shader->getProgram(), GL_LINK_STATUS, &result);
            break;
        default:
            break;
        }
        if (result == GL_FALSE) 
        {
            i32 infoLogLength = -1;
            glGetProgramiv(shader->getProgram(), GL_INFO_LOG_LENGTH, &infoLogLength);
            if (infoLogLength > 0) 
            {
                i32 bufferSize = sizeof(char) * infoLogLength;
                char* infoLog = static_cast<char*>(alloca(bufferSize));
                glGetProgramInfoLog(shader->getProgram(), bufferSize, &infoLogLength, infoLog);
                output.assign(infoLog);
            }
            return false;
        }
        return true;
    }

    static GfxContext* s_ctx = nullptr;

    Shader::Shader(const char* shaderName, const char* shaderFilePath, Type type) 
        : m_name(shaderName), m_source(shaderFilePath), m_type(type) 
    {
        if (s_ctx == nullptr)
        {
            s_ctx = GfxContext::get();
        }

        u32 stringCount = m_source.includes.size() + 1;
        std::vector<const char*> strings{ m_source.src.c_str() };
        for (const auto& string : m_source.includes) 
        {
            strings.push_back(string.c_str());
        }

        /** Node - @min:
        * According to OpenGL wiki:
            GLuint glCreateShaderProgramv(GLenum type​, GLsizei count​, const char **strings​);
            This works exactly as if you took count​ and strings​ strings, created a shader object from them of the type​ shader type, and then linked that shader object into a program with the GL_PROGRAM_SEPARABLE parameter. And then detaching and deleting the shader object.

            This process can fail, just as compilation or linking can fail. The program infolog can thus contain compile errors as well as linking errors.

            Note: glCreateShaderProgramv will return either the name of a program object or zero - independent of which errors might occur during shader compilation or linkage! A return value of zero simply states that either the shader object or program object could not be created. If a non-zero value is returned, you will still need to check the program info logs to make sure compilation and linkage succeeded! Also, the function itself may generate an error under certain conditions which will also result in zero being returned.
            Warning: When linking shaders with separable programs, your shaders must redeclare the gl_PerVertex interface block if you attempt to use any of the variables defined within it.
        */
        switch (m_type) 
        {
        case Type::kVertex:
            glObject = glCreateShaderProgramv(GL_VERTEX_SHADER, stringCount, strings.data());
            break;
        case Type::kPixel:
            glObject = glCreateShaderProgramv(GL_FRAGMENT_SHADER, stringCount, strings.data());
            break;
        case Type::kGeometry:
            glObject = glCreateShaderProgramv(GL_GEOMETRY_SHADER, stringCount, strings.data());
            break;
        case Type::kCompute:
            glObject = glCreateShaderProgramv(GL_COMPUTE_SHADER, stringCount, strings.data());
            break;
        case Type::kInvalid:
        default:
            break;
        }
        std::string linkLog;
        if (!getProgramInfoLog(this, ShaderInfoLogType::kLink, linkLog)) 
        {
            cyanError("%s", linkLog.c_str());
        }

        Shader::initialize(this);
    }

    void Shader::initialize(Shader* shader) 
    {
        GLuint program = shader->getProgram();
        // query list of active uniforms
        i32 activeNumUniforms = -1;
        glGetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &activeNumUniforms);
        if (activeNumUniforms > 0) 
        {
            i32 maxNameLength = -1;
            glGetProgramInterfaceiv(program, GL_UNIFORM, GL_MAX_NAME_LENGTH, &maxNameLength);
            if (maxNameLength > 0)
            {
                char* name = static_cast<char*>(alloca(maxNameLength + 1));
                for (i32 i = 0; i < activeNumUniforms; ++i) {
                    GLenum type; i32 count;
                    glGetActiveUniform(program, i, maxNameLength, nullptr, &count, &type, name);
                    auto translated = translate(type);
                    if (count > 1) 
                    {
                        for (i32 j = 0; j < count; ++j) 
                        {
                            char* nextToken;
                            char* token = strtok_s(name, "[", &nextToken);
                            char suffix[5] = "[%u]";
                            char* format = strcat(token, suffix);
                            sprintf(name, format, j);

                            UniformDesc desc = { };
                            desc.type = translated;
                            desc.name = std::string(name);
                            desc.location = glGetUniformLocation(program, name);
                            shader->m_uniformMap.insert({ desc.name, desc });
                            switch (translated) 
                            {
                            case UniformDesc::Type::kSampler2D:
                                shader->m_textureBindingMap.insert({ desc.name, TextureBinding { desc.name.c_str(), nullptr, -1 } });
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    else
                    {
                        UniformDesc desc = { };
                        desc.type = translated;
                        desc.name = std::string(name);
                        desc.location = glGetUniformLocation(program, name);
                        shader->m_uniformMap.insert({ desc.name, desc });
                        switch (translated) 
                        {
                        case UniformDesc::Type::kSampler2D:
                            shader->m_textureBindingMap.insert({ desc.name, TextureBinding { desc.name.c_str(), nullptr, -1 } });
                            break;
                        default:
                            break;
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
                    ShaderStorageBinding binding = { name, i, nullptr, -1 };
                    shader->m_shaderStorageBindingMap.insert({ std::string(name), binding });
                }
            }
        }
    }

    bool Shader::hasActiveUniform(const char* name)
    {
        return (getUniformLocation(name) >= 0);
    }

    i32 Shader::getUniformLocation(const char* name) 
    {
        auto entry = m_uniformMap.find(std::string(name));
        if (entry == m_uniformMap.end()) 
        {
            return -1;
        }
        return entry->second.location;
    }

#define SET_UNIFORM(func, ...)                \
    i32 location = getUniformLocation(name);  \
    if (location >= 0) {                      \
        func(glObject, location, __VA_ARGS__);\
    }                                         \
    return *this;                             \

    void Shader::bind() 
    {
        glUseProgram(glObject);
    }

    void Shader::unbind() 
    {
        glUseProgram(0);
    }

    Shader& Shader::setUniform(const char* name, f32 data) 
    {
        SET_UNIFORM(glProgramUniform1f, data)
    }

    Shader& Shader::setUniform(const char* name, u32 data) 
    {
        SET_UNIFORM(glProgramUniform1ui, data)
    }

    Shader& Shader::setUniform(const char* name, i32 data) 
    {
        SET_UNIFORM(glProgramUniform1i, data)
    }

    Shader& Shader::setUniform(const char* name, const glm::vec2& data) 
    {
        SET_UNIFORM(glProgramUniform2f, data.x, data.y)
    }

    Shader& Shader::setUniform(const char* name, const glm::uvec2& data)
    {
        SET_UNIFORM(glProgramUniform2ui, data.x, data.y)
    }

    Shader& Shader::setUniform(const char* name, const glm::vec3& data) 
    {
        SET_UNIFORM(glProgramUniform3f, data.x, data.y, data.z)
    }
    
    Shader& Shader::setUniform(const char* name, const glm::vec4& data) 
    {
        SET_UNIFORM(glProgramUniform4f, data.x, data.y, data.z, data.w)
    }

    Shader& Shader::setUniform(const char* name, const glm::mat4& data) 
    {
        SET_UNIFORM(glProgramUniformMatrix4fv, 1, false, &data[0][0])
    }

    Shader& Shader::setUniform(const char* name, const u64& data) 
    {
        SET_UNIFORM(glProgramUniformHandleui64ARB, data);
    }

    Shader& Shader::setUniform(const char* name, const glm::ivec2& data) 
    {
        SET_UNIFORM(glProgramUniform2i, data.x, data.y);
    }

    Shader& Shader::setTexture(const char* samplerName, GfxTexture* texture) 
    {
        auto entry = m_textureBindingMap.find(samplerName);
        if (entry != m_textureBindingMap.end())
        {
            TextureBinding& textureBinding = entry->second;
            // there is already a texture binding unit allocated, only need to update which texture is bound
            if (textureBinding.textureUnit >= 0)
            {
                texture->bind(s_ctx, textureBinding.textureUnit);
            }
            else
            {
                textureBinding.textureUnit = s_ctx->allocTextureUnit();
                setUniform(samplerName, textureBinding.textureUnit);
                texture->bind(s_ctx, textureBinding.textureUnit);
            }
            textureBinding.texture = texture;
        }
        return *this;
    }

    bool Shader::hasShaderStorgeBlock(const char* blockName)
    {
        auto entry = m_shaderStorageBindingMap.find(blockName);
        return (entry != m_shaderStorageBindingMap.end());
    }

    Shader& Shader::setShaderStorageBuffer(ShaderStorageBuffer* buffer)
    {
        auto entry = m_shaderStorageBindingMap.find(buffer->getBlockName());
        assert(entry != m_shaderStorageBindingMap.end());
        ShaderStorageBinding& bufferBinding = entry->second;
        if (bufferBinding.bufferUnit >= 0)
        {
            buffer->bind(s_ctx, bufferBinding.bufferUnit);
        }
        else
        {
            u32 bufferUnit = s_ctx->allocShaderStorageBinding();
            bufferBinding.bufferUnit = bufferUnit;
            buffer->bind(s_ctx, bufferBinding.bufferUnit);
        }
        bufferBinding.buffer = buffer;
        return *this;
    }

    void Shader::unbindTextures(GfxContext* ctx)
    {
        for (auto& entry : m_textureBindingMap)
        {
            auto& textureBinding = entry.second;
            if (textureBinding.textureUnit >= 0)
            {
                textureBinding.texture->unbind(ctx);
                textureBinding.textureUnit = -1;
            }
            else
            {
                // assert(textureBinding.texture == nullptr);
            }
        }
    }

    void Shader::unbindShaderStorageBuffers(GfxContext* ctx)
    {
        for (auto& entry : m_shaderStorageBindingMap)
        {
            auto& bufferBinding = entry.second;
            if (bufferBinding.bufferUnit >= 0)
            {
                bufferBinding.buffer->unbind(ctx);
                bufferBinding.bufferUnit = -1;
            }
            else
            {
                assert(bufferBinding.buffer == nullptr);
            }
        }
    }

    bool ProgramPipeline::isBound()
    {
        return m_bIsBound;
    }

    void ProgramPipeline::bind(GfxContext* ctx)
    {
        ctx->bindProgramPipeline(this);
        m_bIsBound = true;
    }

    void ProgramPipeline::unbind(GfxContext* ctx)
    {
        resetBindings(ctx);
        ctx->unbindProgramPipeline();
        m_bIsBound = false;
    }

    PixelPipeline::PixelPipeline(const char* pipelineName, const char* vsName, const char* psName) 
        : ProgramPipeline(pipelineName)
    {
        m_vertexShader = dynamic_cast<VertexShader*>(ShaderManager::getShader(vsName));
        m_pixelShader = dynamic_cast<PixelShader*>(ShaderManager::getShader(psName));

        initialize();
    }

    PixelPipeline::PixelPipeline(const char* pipelineName, VertexShader* vertexShader, PixelShader* pixelShader) 
        : ProgramPipeline(pipelineName), m_vertexShader(vertexShader), m_pixelShader(pixelShader) 
    {
        initialize();
    }

    bool PixelPipeline::initialize() 
    {
        bool bSuccess = false;
        if (m_vertexShader) 
        {
            glUseProgramStages(glObject, GL_VERTEX_SHADER_BIT, m_vertexShader->getProgram());
        }
        else 
        {
            bSuccess = false;
        }
        if (m_pixelShader) 
        {
            glUseProgramStages(glObject, GL_FRAGMENT_SHADER_BIT, m_pixelShader->getProgram());
        }
        else 
        {
            bSuccess = false;
        }
        return bSuccess;
    }

    void PixelPipeline::resetBindings(GfxContext* ctx)
    {
        m_vertexShader->unbindTextures(ctx);
        m_vertexShader->unbindShaderStorageBuffers(ctx);
        m_pixelShader->unbindTextures(ctx);
        m_pixelShader->unbindShaderStorageBuffers(ctx);
    }

#define PIXEL_PIPELINE_SET_UNIFORM(name, data)      \
        assert(isBound());                          \
        bool bFoundUniform = false;                 \
        if (m_vertexShader->hasActiveUniform(name)) \
        {                                           \
            bFoundUniform |= true;                  \
            m_vertexShader->setUniform(name, data); \
        }                                           \
        if (m_pixelShader->hasActiveUniform(name))  \
        {                                           \
            bFoundUniform |= true;                  \
            m_pixelShader->setUniform(name, data);  \
        }                                           \
        // assert(bFoundUniform);                   \

    void PixelPipeline::setUniform(const char* name, u32 data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, const u64& data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, i32 data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, f32 data) 
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, const glm::ivec2& data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, const glm::uvec2& data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, const glm::vec2& data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, const glm::vec3& data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, const glm::vec4& data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setUniform(const char* name, const glm::mat4& data)
    {
        PIXEL_PIPELINE_SET_UNIFORM(name, data);
    }

    void PixelPipeline::setTexture(const char* samplerName, GfxTexture* texture)
    {
        assert(isBound());
        bool bFoundUniform = false; 
        if (m_vertexShader->hasActiveUniform(samplerName))
        {
            bFoundUniform |= true;
            m_vertexShader->setTexture(samplerName, texture);
        }
        if (m_pixelShader->hasActiveUniform(samplerName))
        {
            bFoundUniform |= true;
            m_pixelShader->setTexture(samplerName, texture);
        }
        // assert(bFoundUniform);
    }

    void PixelPipeline::setShaderStorageBuffer(ShaderStorageBuffer* buffer)
    {
        assert(isBound());
        bool bFound = false;
        if (m_vertexShader->hasShaderStorgeBlock(buffer->getBlockName()))
        {
            bFound |= true;
            m_vertexShader->setShaderStorageBuffer(buffer);
        }
        if (m_pixelShader->hasShaderStorgeBlock(buffer->getBlockName()))
        {
            bFound |= true;
            m_pixelShader->setShaderStorageBuffer(buffer);
        }
        // assert(bFound);
    }

    GeometryPipeline::GeometryPipeline(const char* pipelineName, const char* vsName, const char* gsName, const char* psName) 
        : ProgramPipeline(pipelineName) {
        m_vertexShader = dynamic_cast<VertexShader*>(ShaderManager::getShader(vsName));
        m_geometryShader = dynamic_cast<GeometryShader*>(ShaderManager::getShader(gsName));
        m_pixelShader = dynamic_cast<PixelShader*>(ShaderManager::getShader(psName));
        initialize();
    }

    GeometryPipeline::GeometryPipeline(const char* pipelineName, VertexShader* vertexShader, GeometryShader* geometryShader, PixelShader* pixelShader) 
        : ProgramPipeline(pipelineName), m_vertexShader(vertexShader), m_geometryShader(geometryShader), m_pixelShader(pixelShader) {
        initialize();
    }

    bool GeometryPipeline::initialize() {
        glUseProgramStages(glObject, GL_VERTEX_SHADER_BIT, m_vertexShader->getProgram());
        glUseProgramStages(glObject, GL_GEOMETRY_SHADER, m_geometryShader->getProgram());
        glUseProgramStages(glObject, GL_FRAGMENT_SHADER_BIT, m_pixelShader->getProgram());
        return true;
    }

#define GEOMETRY_PIPELINE_SET_UNIFORM(name, data)       \
        assert(isBound());                              \
        bool bFoundUniform = false;                     \
        if (m_geometryShader->hasActiveUniform(name))   \
        {                                               \
            bFoundUniform |= true;                      \
            m_geometryShader->setUniform(name, data);   \
        }                                               \
        if (m_vertexShader->hasActiveUniform(name))     \
        {                                               \
            bFoundUniform |= true;                      \
            m_vertexShader->setUniform(name, data);     \
        }                                               \
        if (m_pixelShader->hasActiveUniform(name))      \
        {                                               \
            bFoundUniform |= true;                      \
            m_pixelShader->setUniform(name, data);      \
        }                                               \
        // assert(bFoundUniform);                          \

    void GeometryPipeline::setUniform(const char* name, u32 data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, const u64& data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, i32 data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, f32 data) 
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, const glm::ivec2& data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, const glm::uvec2& data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, const glm::vec2& data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, const glm::vec3& data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, const glm::vec4& data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setUniform(const char* name, const glm::mat4& data)
    {
        GEOMETRY_PIPELINE_SET_UNIFORM(name, data);
    }

    void GeometryPipeline::setTexture(const char* samplerName, GfxTexture* texture)
    {
        assert(isBound());
        bool bFoundUniform = false; 
        if (m_geometryShader->hasActiveUniform(samplerName))
        {
            bFoundUniform |= true;
            m_geometryShader->setTexture(samplerName, texture);
        }
        if (m_vertexShader->hasActiveUniform(samplerName))
        {
            bFoundUniform |= true;
            m_vertexShader->setTexture(samplerName, texture);
        }
        if (m_pixelShader->hasActiveUniform(samplerName))
        {
            bFoundUniform |= true;
            m_pixelShader->setTexture(samplerName, texture);
        }
        // assert(bFoundUniform);
    }

    void GeometryPipeline::setShaderStorageBuffer(ShaderStorageBuffer* buffer)
    {
        assert(isBound());
        bool bFound = false;
        if (m_geometryShader->hasShaderStorgeBlock(buffer->getBlockName()))
        {
            bFound |= true;
            m_geometryShader->setShaderStorageBuffer(buffer);
        }
        if (m_vertexShader->hasShaderStorgeBlock(buffer->getBlockName()))
        {
            bFound |= true;
            m_vertexShader->setShaderStorageBuffer(buffer);
        }
        if (m_pixelShader->hasShaderStorgeBlock(buffer->getBlockName()))
        {
            bFound |= true;
            m_pixelShader->setShaderStorageBuffer(buffer);
        }
        // assert(bFound);
    }

    void GeometryPipeline::resetBindings(GfxContext* ctx)
    {

    }

    ComputePipeline::ComputePipeline(const char* pipelineName, const char* csName) 
        : ProgramPipeline(pipelineName) 
    {
        initialize();
    }

    ComputePipeline::ComputePipeline(const char* pipelineName, ComputeShader* computeShader) 
        : ProgramPipeline(pipelineName), m_computeShader(computeShader) {
        initialize();
    }

    bool ComputePipeline::initialize() {
        glUseProgramStages(glObject, GL_COMPUTE_SHADER_BIT, m_computeShader->getProgram());
        return true;
    }

#define COMPUTE_PIPELINE_SET_UNIFORM(name, data)        \
        assert(isBound());                              \
        bool bFoundUniform = false;                     \
        if (m_computeShader->hasActiveUniform(name))    \
        {                                               \
            bFoundUniform |= true;                      \
            m_computeShader->setUniform(name, data);    \
        }                                               \

    void ComputePipeline::setUniform(const char* name, u32 data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, const u64& data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, i32 data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, f32 data) 
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, const glm::ivec2& data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, const glm::uvec2& data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, const glm::vec2& data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, const glm::vec3& data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, const glm::vec4& data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setUniform(const char* name, const glm::mat4& data)
    {
        COMPUTE_PIPELINE_SET_UNIFORM(name, data);
    }

    void ComputePipeline::setTexture(const char* samplerName, GfxTexture* texture)
    {
        assert(isBound());
        bool bFoundUniform = false; 
        if (m_computeShader->hasActiveUniform(samplerName))
        {
            bFoundUniform |= true;
            m_computeShader->setTexture(samplerName, texture);
        }
        // assert(bFoundUniform);
    }

    void ComputePipeline::setShaderStorageBuffer(ShaderStorageBuffer* buffer)
    {
        assert(isBound());
        bool bFound = false;
        if (m_computeShader->hasShaderStorgeBlock(buffer->getBlockName()))
        {
            bFound |= true;
            m_computeShader->setShaderStorageBuffer(buffer);
        }
        // assert(bFound);
    }

    void ComputePipeline::resetBindings(GfxContext* ctx)
    {

    }

    ShaderManager* Singleton<ShaderManager>::singleton = nullptr;

    void ShaderManager::initialize() 
    {

    }

    Shader* ShaderManager::getShader(const char* shaderName) 
    {
        auto entry = singleton->m_shaderMap.find(std::string(shaderName));
        if (entry == singleton->m_shaderMap.end()) 
        {
            return nullptr;
        }
        return entry->second.get();
    }

    PixelPipeline* ShaderManager::createPixelPipeline(const char* pipelineName, VertexShader* vertexShader, PixelShader* pixelShader) 
    {
        if (pipelineName) 
        {
            auto entry = singleton->m_pipelineMap.find(std::string(pipelineName));
            if (entry == singleton->m_pipelineMap.end()) 
            {
                PixelPipeline* pipeline = new PixelPipeline(pipelineName, vertexShader, pixelShader);
                singleton->m_pipelineMap.insert({ std::string(pipelineName), std::unique_ptr<ProgramPipeline>(pipeline) });
                return pipeline;
            }
            else 
            {
                if (PixelPipeline* foundPipeline = dynamic_cast<PixelPipeline*>(entry->second.get())) 
                {
                    return foundPipeline;
                }
            }
        }
        return nullptr;
    }

    ComputePipeline* ShaderManager::createComputePipeline(const char* pipelineName, ComputeShader* computeShader) 
    {
        if (pipelineName) 
        {
            auto entry = singleton->m_pipelineMap.find(std::string(pipelineName));
            if (entry == singleton->m_pipelineMap.end()) 
            {
                ComputePipeline* pipeline = new ComputePipeline(pipelineName, computeShader);
                singleton->m_pipelineMap.insert({ std::string(pipelineName), std::unique_ptr<ProgramPipeline>(pipeline) });
                return pipeline;
            }
            else 
            {
                if (ComputePipeline* foundPipeline = dynamic_cast<ComputePipeline*>(entry->second.get())) 
                {
                    return foundPipeline;
                }
            }
        }
        return nullptr;
    }
}