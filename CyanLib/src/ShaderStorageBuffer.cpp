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
        ctx->bindShaderStorageBuffer(this, inBinding);
        binding = inBinding;
    }

    void ShaderStorageBuffer::unbind(GfxContext* ctx)
    {
        ctx->unbindShaderStorageBuffer(binding);
        binding = -1;
    }

    bool ShaderStorageBuffer::isBound()
    {
        return (binding >= 0);
    }

}