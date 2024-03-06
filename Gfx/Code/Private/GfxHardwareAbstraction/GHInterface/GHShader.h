#pragma once

#include "MathLibrary.h"

namespace Cyan 
{
    class GHTexture;
    class GHRWBuffer;
    class GHAtomicCounterBuffer;

#define DECLARE_SET_UNIFORM_TYPE(valueType)                                \
    virtual void setUniform(const char* name, const valueType& value) = 0; \

#define DECLARE_SET_UNIFORM_INTERFACE()      \
        DECLARE_SET_UNIFORM_TYPE(i32)        \
        DECLARE_SET_UNIFORM_TYPE(u32)        \
        DECLARE_SET_UNIFORM_TYPE(f32)        \
        DECLARE_SET_UNIFORM_TYPE(glm::uvec2) \
        DECLARE_SET_UNIFORM_TYPE(glm::ivec2) \
        DECLARE_SET_UNIFORM_TYPE(glm::ivec3) \
        DECLARE_SET_UNIFORM_TYPE(glm::vec2)  \
        DECLARE_SET_UNIFORM_TYPE(glm::vec3)  \
        DECLARE_SET_UNIFORM_TYPE(glm::vec4)  \
        DECLARE_SET_UNIFORM_TYPE(glm::mat4)  \
        virtual void bindTexture(const char* samplerName, GHTexture* texture, bool& outBound) = 0;              \
        virtual void unbindTexture(const char* samplerName, GHTexture* texture) = 0;                            \
        virtual void bindRWBuffer(const char* bufferName, GHRWBuffer* buffer, bool& outBound) = 0;              \
        virtual void unbindRWBuffer(const char* bufferName) = 0;                                                \
        virtual void bindAtomicCounter(const char* name, GHAtomicCounterBuffer* counter, bool& outBound) = 0;   \
        virtual void unbindAtomicCounter(const char* name) = 0;                                                 \
        virtual void bindRWTexture(const char* name, GHTexture* RWTexture, u32 mipLevel, bool& outBound) = 0;   \

    struct ShaderUniformDesc
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
        // i32 location;
    };
    using ShaderUniformMap = std::unordered_map<std::string, ShaderUniformDesc>;

    class GHShader
    {
    public:

        virtual ~GHShader() { }

        DECLARE_SET_UNIFORM_INTERFACE();

        ShaderUniformMap m_uniformMap;
    };


    class GHVertexShader : public GHShader
    {
    public:
        virtual ~GHVertexShader() { }
    };

    class GHPixelShader : public GHShader
    {
    public:
        virtual ~GHPixelShader() { }
    };

    class GHComputeShader : public GHShader
    {
    public:
        virtual ~GHComputeShader() { }
    };

    class GHGeometryShader : public GHShader
    {
    public:
        virtual ~GHGeometryShader() { }
    };
}
