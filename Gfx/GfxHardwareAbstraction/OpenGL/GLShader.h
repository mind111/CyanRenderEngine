#pragma once

#include "Core.h"
#include "MathLibrary.h"
#include "GLObject.h"
#include "GHShader.h"

namespace Cyan
{
    struct GLShaderUniformDesc
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

    class GLShader : public GLObject, public Shader
    {
    public:
        GLShader(const char* text);
        ~GLShader();

        void build();

    protected:
        i32 getUniformLocation(const char* name);
        GLShader& setUniform(const char* name, u32 data);
        GLShader& setUniform(const char* name, const u64& data);
        GLShader& setUniform(const char* name, i32 data);
        GLShader& setUniform(const char* name, f32 data);
        GLShader& setUniform(const char* name, const glm::ivec2& data);
        GLShader& setUniform(const char* name, const glm::uvec2& data);
        GLShader& setUniform(const char* name, const glm::vec2& data);
        GLShader& setUniform(const char* name, const glm::vec3& data);
        GLShader& setUniform(const char* name, const glm::vec4& data);
        GLShader& setUniform(const char* name, const glm::mat4& data);

        const char* m_text = nullptr;
        std::unordered_map<std::string, GLShaderUniformDesc> m_uniformDescMap;
    };

    class GLVertexShader : public GLShader, public GHVertexShader
    {
    public:
        GLVertexShader(const char* text);
        ~GLVertexShader() { }
    };

    class GLPixelShader : public GLShader, public GHPixelShader
    {
    public:
        GLPixelShader(const char* text);
        ~GLPixelShader() { }
    };

    class GLComputeShader : public GLShader, public GHComputeShader
    {
    public:
        GLComputeShader(const char* text);
        ~GLComputeShader() { }
    };
}
