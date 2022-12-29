#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>

#include "gtc/type_ptr.hpp"

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

    void Shader::initialize(Shader* shader) {
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
                                shader->m_samplerBindingMap.insert({ desc.name, nullptr });
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
                    shader->m_shaderStorageBlockMap.insert({ std::string(name), i });
                }
            }
        }
    }

    i32 Shader::getUniformLocation(const char* name) {
        auto entry = m_uniformMap.find(std::string(name));
        if (entry == m_uniformMap.end()) {
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

    void Shader::bind() {
        glUseProgram(glObject);
    }

    void Shader::unbind() {
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

    Shader& Shader::setTexture(const char* samplerName, ITextureRenderable* texture) 
    {
        if (texture) 
        {
            m_samplerBindingMap[std::string(samplerName)] = texture;
        }
        return *this;
    }

    PixelPipeline::PixelPipeline(const char* pipelineName, const char* vsName, const char* psName) 
        : PipelineStateObject(pipelineName) {
        m_vertexShader = dynamic_cast<VertexShader*>(ShaderManager::getShader(vsName));
        m_pixelShader = dynamic_cast<PixelShader*>(ShaderManager::getShader(psName));
        initialize();
    }

    PixelPipeline::PixelPipeline(const char* pipelineName, VertexShader* vertexShader, PixelShader* pixelShader) 
        : PipelineStateObject(pipelineName), m_vertexShader(vertexShader), m_pixelShader(pixelShader) {
        initialize();
    }

    bool PixelPipeline::initialize() {
        bool bSuccess = false;
        if (m_vertexShader) {
            glUseProgramStages(glObject, GL_VERTEX_SHADER_BIT, m_vertexShader->getProgram());
        }
        else {
            bSuccess = false;
        }
        if (m_pixelShader) {
            glUseProgramStages(glObject, GL_FRAGMENT_SHADER_BIT, m_pixelShader->getProgram());
        }
        else {
            bSuccess = false;
        }
        return bSuccess;
    }

    GeometryPipeline::GeometryPipeline(const char* pipelineName, const char* vsName, const char* gsName, const char* psName) 
        : PipelineStateObject(pipelineName) {
        m_vertexShader = dynamic_cast<VertexShader*>(ShaderManager::getShader(vsName));
        m_geometryShader = dynamic_cast<GeometryShader*>(ShaderManager::getShader(gsName));
        m_pixelShader = dynamic_cast<PixelShader*>(ShaderManager::getShader(psName));
        initialize();
    }

    GeometryPipeline::GeometryPipeline(const char* pipelineName, VertexShader* vertexShader, GeometryShader* geometryShader, PixelShader* pixelShader) 
        : PipelineStateObject(pipelineName), m_vertexShader(vertexShader), m_geometryShader(geometryShader), m_pixelShader(pixelShader) {
        initialize();
    }

    bool GeometryPipeline::initialize() {
        glUseProgramStages(glObject, GL_VERTEX_SHADER_BIT, m_vertexShader->getProgram());
        glUseProgramStages(glObject, GL_GEOMETRY_SHADER, m_geometryShader->getProgram());
        glUseProgramStages(glObject, GL_FRAGMENT_SHADER_BIT, m_pixelShader->getProgram());
        return true;
    }

    ComputePipeline::ComputePipeline(const char* pipelineName, const char* csName) 
        : PipelineStateObject(pipelineName) {
        initialize();
    }

    ComputePipeline::ComputePipeline(const char* pipelineName, ComputeShader* computeShader) 
        : PipelineStateObject(pipelineName), m_computeShader(computeShader) {
        initialize();
    }

    bool ComputePipeline::initialize() {
        glUseProgramStages(glObject, GL_COMPUTE_SHADER_BIT, m_computeShader->getProgram());
        return true;
    }

    ShaderManager::ShaderManager() {

    }

    ShaderManager* Singleton<ShaderManager>::singleton = nullptr;
    void ShaderManager::initialize() {

    }

    Shader* ShaderManager::getShader(const char* shaderName) {
        auto entry = singleton->m_shaderMap.find(std::string(shaderName));
        if (entry == singleton->m_shaderMap.end()) {
            return nullptr;
        }
        return entry->second.get();
    }

    PixelPipeline* ShaderManager::createPixelPipeline(const char* pipelineName, VertexShader* vertexShader, PixelShader* pixelShader) {
        if (pipelineName) {
            auto entry = singleton->m_pipelineMap.find(std::string(pipelineName));
            if (entry == singleton->m_pipelineMap.end()) {
                PixelPipeline* pipeline = new PixelPipeline(pipelineName, vertexShader, pixelShader);
                singleton->m_pipelineMap.insert({ std::string(pipelineName), std::unique_ptr<PipelineStateObject>(pipeline) });
                return pipeline;
            }
            else {
                if (PixelPipeline* foundPipeline = dynamic_cast<PixelPipeline*>(entry->second.get())) {
                    return foundPipeline;
                }
            }
        }
        return nullptr;
    }

    ComputePipeline* ShaderManager::createComputePipeline(const char* pipelineName, ComputeShader* computeShader) {
        if (pipelineName) {
            auto entry = singleton->m_pipelineMap.find(std::string(pipelineName));
            if (entry == singleton->m_pipelineMap.end()) {
                ComputePipeline* pipeline = new ComputePipeline(pipelineName, computeShader);
                singleton->m_pipelineMap.insert({ std::string(pipelineName), std::unique_ptr<PipelineStateObject>(pipeline) });
                return pipeline;
            }
            else {
                if (ComputePipeline* foundPipeline = dynamic_cast<ComputePipeline*>(entry->second.get())) {
                    return foundPipeline;
                }
            }
        }
        return nullptr;
    }
}