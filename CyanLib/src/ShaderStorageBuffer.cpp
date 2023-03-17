#include "ShaderStorageBuffer.h"
#include "GfxContext.h"

namespace Cyan
{
    ShaderStorageBuffer::~ShaderStorageBuffer() 
    { 
        if (isBound())
        {
            unbind(GfxContext::get());
        }

        GLuint buffers[] = { getGpuResource() };
        glDeleteBuffers(1, buffers);
    }

    void ShaderStorageBuffer::bind(GfxContext* ctx, u32 inBinding) 
    { 
        ctx->setShaderStorageBuffer(this, inBinding);
        binding = inBinding;
    }

    void ShaderStorageBuffer::unbind(GfxContext* ctx)
    {
        ctx->setShaderStorageBuffer(nullptr, binding);
        binding = -1;
    }

    bool ShaderStorageBuffer::isBound()
    {
        return (binding >= 0);
    }

}