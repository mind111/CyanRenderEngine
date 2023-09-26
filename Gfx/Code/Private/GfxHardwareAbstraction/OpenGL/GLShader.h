#pragma once

#include "Core.h"
#include "MathLibrary.h"
#include "GLObject.h"
#include "GfxHardwareAbstraction/GHInterface/GHShader.h"

namespace Cyan
{
#define IMPLEMENT_SET_UNIFORM_TYPE(valueType)                                    \
    virtual void setUniform(const char* name, const valueType& value) override { \
        glSetUniform(name, value);                                               \
    }                                                                            \

#define IMPLEMENT_SET_UNIFORM_INTERFACE()      \
        IMPLEMENT_SET_UNIFORM_TYPE(i32)        \
        IMPLEMENT_SET_UNIFORM_TYPE(u32)        \
        IMPLEMENT_SET_UNIFORM_TYPE(f32)        \
        IMPLEMENT_SET_UNIFORM_TYPE(glm::uvec2) \
        IMPLEMENT_SET_UNIFORM_TYPE(glm::ivec2) \
        IMPLEMENT_SET_UNIFORM_TYPE(glm::vec2)  \
        IMPLEMENT_SET_UNIFORM_TYPE(glm::vec3)  \
        IMPLEMENT_SET_UNIFORM_TYPE(glm::vec4)  \
        IMPLEMENT_SET_UNIFORM_TYPE(glm::mat4)  \
        virtual void bindTexture(const char* samplerName, GHTexture* texture, bool& outBound) override {    \
            GLShader::bindTexture(samplerName, texture, outBound);                                          \
        }                                                                                                   \
        virtual void unbindTexture(const char* samplerName, GHTexture* texture) override {                  \
            GLShader::unbindTexture(samplerName, texture);                                                  \
        }                                                                                                   \

    class GLShader : public GLObject
    {
    public:
        GLShader(const std::string& text);
        ~GLShader();

        void build(ShaderUniformMap& uniformMap);

    protected:
        i32 getUniformLocation(const char* name);
        void glSetUniform(const char* name, u32 data);
        void glSetUniform(const char* name, i32 data);
        void glSetUniform(const char* name, f32 data);
        void glSetUniform(const char* name, const u64& data);
        void glSetUniform(const char* name, const glm::ivec2& data);
        void glSetUniform(const char* name, const glm::uvec2& data);
        void glSetUniform(const char* name, const glm::vec2& data);
        void glSetUniform(const char* name, const glm::vec3& data);
        void glSetUniform(const char* name, const glm::vec4& data);
        void glSetUniform(const char* name, const glm::mat4& data);
        void bindTexture(const char* samplerName, GHTexture* texture, bool& outBound);
        void unbindTexture(const char* samplerName, GHTexture* texture);

        std::string m_text;
        std::unordered_map<std::string, i32> m_uniformLocationMap;
    };

    class GLVertexShader : public GLShader, public GHVertexShader
    {
    public:
        GLVertexShader(const std::string& text);
        ~GLVertexShader() { }

        IMPLEMENT_SET_UNIFORM_INTERFACE()
    };

    class GLPixelShader : public GLShader, public GHPixelShader
    {
    public:
        GLPixelShader(const std::string& text);
        ~GLPixelShader() { }

        IMPLEMENT_SET_UNIFORM_INTERFACE()
    };

    class GLComputeShader : public GLShader, public GHComputeShader
    {
    public:
        GLComputeShader(const std::string& text);
        ~GLComputeShader() { }

        IMPLEMENT_SET_UNIFORM_INTERFACE()
    };
}
