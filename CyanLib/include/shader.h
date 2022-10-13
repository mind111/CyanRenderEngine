#pragma once

#include "Windows.h"

#include <memory>
#include <vector>
#include <unordered_map>

#include "glew.h"

#include "Uniform.h"
#include "Texture.h"

#define SHADER_SOURCE_PATH "C:/dev/cyanRenderEngine/shader/"

// Shader storage buffer
struct RegularBuffer
{
    const void* m_data;
    u32         m_totalSize;
    u32         m_sizeToUpload;
    GLuint m_binding;
    GLuint m_ssbo;
};

namespace Cyan
{
    struct ShaderSource
    {
        enum class Type
        {
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

    class Shader : public GpuResource
    {
    public:
        struct UniformMetaData
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

    private:
        void build();
        void buildVsPs();
        void buildVsGsPs();
        void buildCs();

        // do shader introspection to initialize all the meta data
        void init();

        const u32 kMaxNumTextureBindings = 32;
        i32 nextTextureBinding = 0u;
        i32 boundTextures = 0u;
        // std::unordered_map<const char*, u32> uniformLocationMap;
        std::unordered_map<std::string, ITextureRenderable*> samplerBindingMap;
        std::unordered_map<std::string, UniformMetaData> uniformMetaDataMap;
        ShaderSource source;
        GLuint program;
    };

    class ShaderManager : public Singleton<ShaderManager>
    {
    public:
        using ShaderSourceMap = std::unordered_map<std::string, ShaderSource>;
        using ShaderMap = std::unordered_map<std::string, std::unique_ptr<Shader>>;

        ShaderManager() 
            : Singleton()
        { }

        void initialize();
        void finalize() { }

        static Shader* createShader(const ShaderSource& shaderSourceMetaData);

        static Shader* getShader(const char* shaderName);

    private:
        static ShaderSourceMap m_shaderSourceMap;
        static ShaderMap m_shaderMap;
    };
}
