#pragma once

#include <glew/glew.h>

#include "Common.h"

namespace Cyan 
{
    class GpuResource 
    {
    public:
        GpuResource()
            : glObject(-1) 
        { }

        u32 getGpuResource() const { return glObject; }
        // todo: this operator is not robust as object id can get recycled, so same id doesn't guarantee same object
        bool operator==(const GpuResource& rhs) { return glObject == rhs.getGpuResource(); }

    protected:
        GLuint glObject;
    };
}
