#pragma once

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
    struct ShaderStorageBuffer;

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

    class Shader : public GfxResource 
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

        struct TextureBinding
        {
            const char* samplerName = nullptr;
            GfxTexture* texture = nullptr;
            i32 textureUnit = -1;
        };

        struct ShaderStorageBinding
        {
            const char* blockName = nullptr;
            i32 blockIndex = -1;
            ShaderStorageBuffer* buffer = nullptr;
            i32 bufferUnit = -1;
        };

        friend class GfxContext;

        Shader(const char* shaderName, const char* shaderFilePath, Type type = Type::kInvalid);
        virtual ~Shader() { }

        GLuint getProgram() { return getGpuResource(); }
        void bind();
        void unbind();

        // todo: do type checking to make sure type of 'data' matches uniform type defined in shader
        i32 getUniformLocation(const char* name);
        bool hasActiveUniform(const char* name);
        bool hasShaderStorgeBlock(const char* blockName);
        Shader& setUniform(const char* name, u32 data);
        Shader& setUniform(const char* name, const u64& data);
        Shader& setUniform(const char* name, i32 data);
        Shader& setUniform(const char* name, f32 data);
        Shader& setUniform(const char* name, const glm::ivec2& data);
        Shader& setUniform(const char* name, const glm::uvec2& data);
        Shader& setUniform(const char* name, const glm::vec2& data);
        Shader& setUniform(const char* name, const glm::vec3& data);
        Shader& setUniform(const char* name, const glm::vec4& data);
        Shader& setUniform(const char* name, const glm::mat4& data);
        Shader& setTexture(const char* samplerName, GfxTexture* texture);
        Shader& setShaderStorageBuffer(ShaderStorageBuffer* buffer);
        void unbindTextures(GfxContext* ctx);
        void unbindShaderStorageBuffers(GfxContext* ctx);

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

        std::unordered_map<std::string, UniformDesc> m_uniformMap;
        std::unordered_map<std::string, TextureBinding> m_textureBindingMap;
        std::unordered_map<std::string, ShaderStorageBinding> m_shaderStorageBindingMap;
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
    class ProgramPipeline : public GfxResource 
    {
    public:
        ProgramPipeline(const char* pipelineName) 
            : m_name(pipelineName) 
        {
            glCreateProgramPipelines(1, &glObject);
        }
        virtual ~ProgramPipeline() {}

        bool isBound();
        void bind(GfxContext* ctx);
        void unbind(GfxContext* ctx);
        virtual void resetBindings(GfxContext* ctx) { }

        virtual void setUniform(const char* name, u32 data) = 0;
        virtual void setUniform(const char* name, const u64& data) = 0;
        virtual void setUniform(const char* name, i32 data) = 0; 
        virtual void setUniform(const char* name, f32 data) = 0;
        virtual void setUniform(const char* name, const glm::ivec2& data) = 0;
        virtual void setUniform(const char* name, const glm::uvec2& data) = 0;
        virtual void setUniform(const char* name, const glm::vec2& data) = 0;
        virtual void setUniform(const char* name, const glm::vec3& data) = 0;
        virtual void setUniform(const char* name, const glm::vec4& data) = 0;
        virtual void setUniform(const char* name, const glm::mat4& data) = 0;
        virtual void setTexture(const char* samplerName, GfxTexture* texture) = 0;
        virtual void setShaderStorageBuffer(ShaderStorageBuffer* buffer) = 0;
        
        std::string m_name;
    protected:
        virtual bool initialize() { return true; }
        bool m_bIsBound = false;
    };

    class PixelPipeline : public ProgramPipeline 
    {
    public:
        PixelPipeline(const char* pipelineName, const char* vsName, const char* psName);
        PixelPipeline(const char* pipelineName, VertexShader* vertexShader, PixelShader* pixelShader);

        virtual void resetBindings(GfxContext* ctx) override;

        virtual void setUniform(const char* name, u32 data) override;
        virtual void setUniform(const char* name, const u64& data) override;
        virtual void setUniform(const char* name, i32 data) override; 
        virtual void setUniform(const char* name, f32 data) override;
        virtual void setUniform(const char* name, const glm::ivec2& data) override;
        virtual void setUniform(const char* name, const glm::uvec2& data) override;
        virtual void setUniform(const char* name, const glm::vec2& data) override;
        virtual void setUniform(const char* name, const glm::vec3& data) override;
        virtual void setUniform(const char* name, const glm::vec4& data) override;
        virtual void setUniform(const char* name, const glm::mat4& data) override;
        virtual void setTexture(const char* samplerName, GfxTexture* texture) override;
        virtual void setShaderStorageBuffer(ShaderStorageBuffer* buffer) override;

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

        virtual void resetBindings(GfxContext* ctx) override;

        virtual void setUniform(const char* name, u32 data) override;
        virtual void setUniform(const char* name, const u64& data) override;
        virtual void setUniform(const char* name, i32 data) override; 
        virtual void setUniform(const char* name, f32 data) override;
        virtual void setUniform(const char* name, const glm::ivec2& data) override;
        virtual void setUniform(const char* name, const glm::uvec2& data) override;
        virtual void setUniform(const char* name, const glm::vec2& data) override;
        virtual void setUniform(const char* name, const glm::vec3& data) override;
        virtual void setUniform(const char* name, const glm::vec4& data) override;
        virtual void setUniform(const char* name, const glm::mat4& data) override;
        virtual void setTexture(const char* samplerName, GfxTexture* texture) override;
        virtual void setShaderStorageBuffer(ShaderStorageBuffer* buffer) override;

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

        virtual void resetBindings(GfxContext* ctx) override;

        virtual void setUniform(const char* name, u32 data) override;
        virtual void setUniform(const char* name, const u64& data) override;
        virtual void setUniform(const char* name, i32 data) override; 
        virtual void setUniform(const char* name, f32 data) override;
        virtual void setUniform(const char* name, const glm::ivec2& data) override;
        virtual void setUniform(const char* name, const glm::uvec2& data) override;
        virtual void setUniform(const char* name, const glm::vec2& data) override;
        virtual void setUniform(const char* name, const glm::vec3& data) override;
        virtual void setUniform(const char* name, const glm::vec4& data) override;
        virtual void setUniform(const char* name, const glm::mat4& data) override;
        virtual void setTexture(const char* samplerName, GfxTexture* texture) override;
        virtual void setShaderStorageBuffer(ShaderStorageBuffer* buffer) override;

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
