#pragma once

#include "Windows.h"

#include <memory>
#include <vector>
#include <unordered_map>

#include "glew.h"

#include "Uniform.h"
#include "Texture.h"

#define SHADER_SOURCE_PATH "C:/dev/cyanRenderEngine/shader/"

namespace Cyan {
    struct ShaderSource {
        enum class Type {
            kVsPs = 0,
            kVsGsPs,
            kCs
        } type;

        const char* name = nullptr;
        const char* vsFilePath = nullptr;
        const char* psFilePath = nullptr;
        const char* gsFilePath = nullptr;
        const char* csFilePath = nullptr;
    };

    using ShaderType = ShaderSource::Type;

    class Shader : public GpuObject {
    public:
        struct UniformDesc {
            enum class Type {
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

        Shader(const ShaderSource& shaderSource);

        i32 getUniformLocation(const char* name);

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
        Shader& setTexture(const char* samplerName, ITextureRenderable* texture);

        std::string name;
    private:
        void build();
        void buildVsPs();
        void buildVsGsPs();
        void buildCs();

        // do shader introspection to initialize all the meta data
        void init();

        std::unordered_map<std::string, ITextureRenderable*> samplerBindingMap;
        std::unordered_map<std::string, UniformDesc> uniformMetaDataMap;
        std::unordered_map<std::string, u32> shaderStorageBlockMap;
        const u32 kMaxNumTextureBindings = 32;
        i32 nextTextureBinding = 0u;
        i32 boundTextures = 0u;
        ShaderSource source;
        // GLuint program;
    };

    class VertexShader : public Shader {

    };

    class PixelShader {
        std::vector<std::string> includes;
        std::string src;
    };

    class PipelineStateObject : public GpuObject {

    };

    class PixelPipeline : public PipelineStateObject {
        PixelPipeline(const char* vsName, const char* psName) {

        }

        PixelPipeline(VertexShader* vs, PixelShader* ps) {

        }

        VertexShader* vertexShader = nullptr;
        PixelShader* pixelShader = nullptr;
    };

    class GeometryPipeline : public Shader {

    };
    
    class ComputePipeline : public PipelineStateObject {

    };

    class ShaderManager : public Singleton<ShaderManager> {
    public:
        using ShaderSourceMap = std::unordered_map<std::string, ShaderSource>;
        using ShaderMap = std::unordered_map<std::string, std::unique_ptr<Shader>>;

        ShaderManager() 
            : Singleton()
        { }

        void initialize();
        void deinitialize() { }

        static Shader* createShader(const ShaderSource& shaderSourceMetaData);
        static Shader* getShader(const char* shaderName);
    private:
        static ShaderSourceMap m_shaderSourceMap;
        static ShaderMap m_shaderMap;
    };
}
