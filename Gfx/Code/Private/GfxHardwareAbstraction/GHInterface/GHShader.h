#pragma once

#include "MathLibrary.h"

namespace Cyan 
{
    class GHTexture;
#define DECLARE_SET_UNIFORM_TYPE(valueType)                                \
    virtual void setUniform(const char* name, const valueType& value) = 0; \

#define DECLARE_SET_UNIFORM_INTERFACE()      \
        DECLARE_SET_UNIFORM_TYPE(i32)        \
        DECLARE_SET_UNIFORM_TYPE(u32)        \
        DECLARE_SET_UNIFORM_TYPE(f32)        \
        DECLARE_SET_UNIFORM_TYPE(glm::uvec2) \
        DECLARE_SET_UNIFORM_TYPE(glm::ivec2) \
        DECLARE_SET_UNIFORM_TYPE(glm::vec2)  \
        DECLARE_SET_UNIFORM_TYPE(glm::vec3)  \
        DECLARE_SET_UNIFORM_TYPE(glm::mat4)  \
        virtual void bindTexture(const char* samplerName, GHTexture* texture) = 0; \
        virtual void unbindTexture(const char* samplerName, GHTexture* texture) = 0; \

    class GHShader
    {
    public:
        virtual ~GHShader() { }

        DECLARE_SET_UNIFORM_INTERFACE();
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
}
