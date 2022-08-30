#pragma once

#include "glew.h"

#include "Common.h"

#define BINDLESS_TEXTURE 1

namespace Cyan
{
    class GpuResource
    {
    public:
        GpuResource()
            : glResource(-1) 
        { }

        u32 getGpuResource() const { return glResource; }
        bool operator==(const GpuResource& rhs) { return glResource == rhs.getGpuResource(); }

    protected:
        GLuint glResource;
    };
}
