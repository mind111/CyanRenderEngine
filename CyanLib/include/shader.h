﻿#pragma once

#include "Windows.h"

#include <memory>
#include <vector>
#include <array>
#include <unordered_map>
#include <fstream>

#include <glew/glew.h>

#include "Singleton.h"
#include "CyanCore.h"
#include "Texture.h"

// todo: this shouldn't be hard coded
#define SHADER_SOURCE_PATH "C:/dev/cyanRenderEngine/shader/"

namespace Cyan 
{
    class GfxContext;

    struct ShaderSource 
    {
        ShaderSource(const char* shaderFilePath);
        ~ShaderSource() { }

        std::vector<std::string> includes;
        std::string src;
        // only for debugging purpose
        std::string merged;

        void include(const std::string& header) 
        {
            includes.push_back(header);
        }
    };

    class Shader : public GpuResource 
    {
    public:
        enum class Type
        {
            kVertex = 0,
            kPixel,
            kGeometry,
            kCompute,
            kInvalid
        };

        struct UniformDesc 
        {
            enum class Type 
            {
                kInt,
                kUint,
                kFloat,
                kVec2,
                kVec3,
                kVec4,
                kMat4,
                kSampler2D,
                kSampler2DArray,
                kSampler3D,
                kSamplerCube,
                kSamplerShadow,
                kImage3D,
                kImageUI3D,
                kAtomicUint,
                kCount
            } type;
            std::string name;
            i32 location;
        };

        friend class GfxContext;

        Shader(const char* shaderName, const char* shaderFilePath, Type type = Type::kInvalid) 
            : m_name(shaderName), m_source(shaderFilePath), m_type(type) 
        {
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
        virtual ~Shader() { }

        GLuint getProgram() { return getGpuResource(); }
        void bind();
        void unbind();

        // todo: do type checking to make sure type of 'data' matches uniform type defined in shader
        Shader& setUniform(const char* name, u32 data);
        Shader& setUniform(const char* name, const u64& data);
        Shader& setUniform(const char* name, i32 data);
        Shader& setUniform(const char* name, f32 data);
        Shader& setUniform(const char* name, const glm::ivec2& data);
        Shader& setUniform(const char* name, const glm::vec2& data);
        Shader& setUniform(const char* name, const glm::vec3& data);
        Shader& setUniform(const char* name, const glm::vec4& data);
        Shader& setUniform(const char* name, const glm::mat4& data);
        Shader& setTexture(const char* samplerName, GfxTexture* texture);

        std::string m_name;
        ShaderSource m_source;
        Type m_type = Type::kInvalid;
    protected:
        enum class ShaderInfoLogType 
        {
            kCompile,
            kLink
        };

        static bool getProgramInfoLog(Shader* shader, ShaderInfoLogType type, std::string& output);
        static bool compile(Shader* shader, std::string& output);
        /**
        * shader info introspection
        */
        static void initialize(Shader* shader);

        i32 getUniformLocation(const char* name);

        std::unordered_map<std::string, UniformDesc> m_uniformMap;
        std::unordered_map<std::string, GfxTexture*> m_samplerBindingMap;
        std::unordered_map<std::string, u32> m_shaderStorageBlockMap;
    };

    class VertexShader : public Shader 
    {
    public:
        VertexShader(const char* shaderName, const char* shaderFilePath) 
            : Shader(shaderName, shaderFilePath, Type::kVertex) 
        {
        }
    };

    class PixelShader : public Shader 
    {
    public:
        PixelShader(const char* shaderName, const char* shaderFilePath) 
            : Shader(shaderName, shaderFilePath, Type::kPixel) 
        {
        }
    };

    class GeometryShader : public Shader 
    {
    public:
        GeometryShader(const char* shaderName, const char* shaderFilePath) 
            : Shader(shaderName, shaderFilePath, Type::kGeometry) 
        {
        }
    };

    class ComputeShader : public Shader 
    {
    public:
        ComputeShader(const char* shaderName, const char* shaderFilePath) 
            : Shader(shaderName, shaderFilePath, Type::kCompute) 
        {
        }
    };

    // todo: maybe other gfx pipeline states can be encapsulated together with shader...? such as depth test, blending, and bluh bluh bluh
    class ProgramPipeline : public GpuResource 
    {
    public:
        ProgramPipeline(const char* pipelineName) 
            : m_name(pipelineName) 
        {
            glCreateProgramPipelines(1, &glObject);
        }
        virtual ~ProgramPipeline() {}

        void bind() 
        {
            glBindProgramPipeline(glObject);
        }

        void unbind()
        {
            glBindProgramPipeline(0);
        }
        
        std::string m_name;
    protected:
        virtual bool initialize() { return true; }
    };

    struct RenderTarget
    {
        RenderTarget() = default;
        RenderTarget(GfxTexture2D* inTexture, u32 inMip = 0, const glm::vec4& inClearColor = glm::vec4(0.f, 0.f, 0.f, 1.f)) 
            : texture(inTexture), mip(inMip), clearColor(inClearColor)
        { 
            if (texture)
            {
                width = max(inTexture->width / pow(2, mip), width);
                height = max(inTexture->height / pow(2, mip), height);
                assert(width > 0);
                assert(height > 0);
            }
        }

        RenderTarget(TextureCube* inTexture, u32 inLayer, u32 inMip = 0, const glm::vec4& inClearColor = glm::vec4(0.f, 0.f, 0.f, 1.f)) 
            : texture(inTexture), mip(inMip), layer(inLayer), clearColor(inClearColor)
        { 
            if (texture)
            {
                width = max(inTexture->resolution / pow(2, mip), width);
                height = max(inTexture->resolution / pow(2, mip), height);
                assert(width > 0);
                assert(height > 0);
            }
        }

        GfxTexture* texture = nullptr;
        u32 width = 1;
        u32 height = 1;
        u32 mip = 0;
        u32 layer = 0; // this is only used if texture is of type GfxTextureCube
        glm::vec4 clearColor = glm::vec4(0.f, 0.f, 0.f, 1.f);
    };

    class PixelPipeline : public ProgramPipeline 
    {
    public:
        PixelPipeline(const char* pipelineName, const char* vsName, const char* psName);
        PixelPipeline(const char* pipelineName, VertexShader* vertexShader, PixelShader* pixelShader);

        VertexShader* m_vertexShader = nullptr;
        PixelShader* m_pixelShader = nullptr;
    protected:
        virtual bool initialize() override;
    };

    class GeometryPipeline : public ProgramPipeline 
    {
    public:
        GeometryPipeline(const char* pipelineName, const char* vsName, const char* gsName, const char* psName);
        GeometryPipeline(const char* pipelineName, VertexShader* vertexShader, GeometryShader* geometryShader, PixelShader* pixelShader);

        VertexShader* m_vertexShader = nullptr;
        GeometryShader* m_geometryShader = nullptr;
        PixelShader* m_pixelShader = nullptr;
    protected:
        virtual bool initialize() override;
    };
    
    class ComputePipeline : public ProgramPipeline 
    {
    public:
        ComputePipeline(const char* pipelineName, const char* csName);
        ComputePipeline(const char* pipelineName, ComputeShader* computeShader);
        ComputeShader* m_computeShader = nullptr;
    protected:
        virtual bool initialize() override;
    };

    class ShaderManager : public Singleton<ShaderManager>
    {
    public:
        using ShaderMap = std::unordered_map<std::string, std::unique_ptr<Shader>>;
        using PipelineMap = std::unordered_map<std::string, std::unique_ptr<ProgramPipeline>>;

        ShaderManager() {}
        ~ShaderManager() {}

        void initialize();
        void deinitialize() { }

        static Shader* getShader(const char* shaderName);

        template <typename ShaderType>
        static ShaderType* createShader(const char* shaderName, const char* shaderFilePath) 
        {
            if (shaderName) 
            {
                auto entry = singleton->m_shaderMap.find(std::string(shaderName));
                if (entry == singleton->m_shaderMap.end()) 
                {
                    ShaderType* shader = new ShaderType(shaderName, shaderFilePath);
                    singleton->m_shaderMap.insert({ std::string(shaderName), std::unique_ptr<ShaderType>(shader) });
                    return shader;
                }
                else 
                {
                    if (ShaderType* foundShader = dynamic_cast<ShaderType*>(entry->second.get())) 
                    {
                        return foundShader;
                    }
                }
            }
            return nullptr;
        }

        static PixelPipeline* createPixelPipeline(const char* pipelineName, VertexShader* vertexShader, PixelShader* pixelShader);
        static ComputePipeline* createComputePipeline(const char* pipelineName, ComputeShader* computeShader);

    private:
        ShaderMap m_shaderMap;
        PipelineMap m_pipelineMap;
    };

#define CreateVS(vs, shaderName, file) auto vs = Cyan::ShaderManager::createShader<Cyan::VertexShader>(shaderName, file);
#define CreatePS(ps, shaderName, file) auto ps = Cyan::ShaderManager::createShader<Cyan::PixelShader>(shaderName, file);
#define CreateCS(cs, shaderName, file) auto cs = Cyan::ShaderManager::createShader<Cyan::ComputeShader>(shaderName, file);
#define CreatePixelPipeline(pipeline, pipelineName, vs, ps) auto pipeline = Cyan::ShaderManager::createPixelPipeline(pipelineName, vs, ps);
#define CreateComputePipeline(pipeline, pipelineName, cs) auto pipeline = Cyan::ShaderManager::createComputePipeline(pipelineName, cs);
}
