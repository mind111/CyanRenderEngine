#pragma once

#include "glew.h"

#include "Common.h"

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
