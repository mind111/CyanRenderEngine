#include <cassert>
#include "IndexBuffer.h"

namespace Cyan
{
    IndexBuffer::IndexBuffer(const std::vector<u32>& indices)
        : GpuResource()
    {
        u32 sizeInBytes = sizeof(indices[0]) * indices.size();
        assert(sizeInBytes > 0);

        glCreateBuffers(1, &glObject);
        glNamedBufferData(getGpuResource(), sizeInBytes, indices.data(), GL_STATIC_DRAW);
    }

    IndexBuffer::~IndexBuffer()
    {
        GLuint buffers[] = { getGpuResource() };
        glDeleteBuffers(1, buffers);
    }
}