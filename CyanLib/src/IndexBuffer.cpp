#include <cassert>
#include "IndexBuffer.h"

namespace Cyan
{
    IndexBuffer::IndexBuffer(const std::vector<u32>& indices)
        : GfxResource()
    {
        u32 sizeInBytes = sizeof(indices[0]) * indices.size();
        assert(sizeInBytes > 0);

        glCreateBuffers(1, &glObject);
        glNamedBufferData(getGpuResource(), sizeInBytes, indices.data(), GL_STATIC_DRAW);
    }

    IndexBuffer::~IndexBuffer()
    {
        GLuint m_buffers[] = { getGpuResource() };
        glDeleteBuffers(1, m_buffers);
    }
}