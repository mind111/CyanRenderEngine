#include "GHBuffer.h"
#include "GfxHardwareContext.h"

namespace Cyan
{
    GHVertexBuffer* GHVertexBuffer::create()
    {
        return nullptr;
    }

    GHIndexBuffer* GHIndexBuffer::create()
    {
        return nullptr;
    }

    std::unique_ptr<GHRWBuffer> GHRWBuffer::create(u32 sizeInBytes)
    {
        std::unique_ptr<GHRWBuffer> outRWBuffer = std::move(GfxHardwareContext::get()->createRWBuffer(sizeInBytes));
        return std::move(outRWBuffer);
    }

    std::unique_ptr<Cyan::GHAtomicCounterBuffer> GHAtomicCounterBuffer::create()
    {
        std::unique_ptr<GHAtomicCounterBuffer> outAtomicCounter = std::move(GfxHardwareContext::get()->createAtomicCounterBuffer());
        return std::move(outAtomicCounter);
    }
}