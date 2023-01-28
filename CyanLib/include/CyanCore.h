#pragma once

#include <glew/glew.h>

#include "Common.h"

namespace Cyan 
{
    class GpuObject 
    {
    public:
        GpuObject()
            : glObject(-1) 
        { }

        u32 getGpuObject() const { return glObject; }
        bool operator==(const GpuObject& rhs) { return glObject == rhs.getGpuObject(); }

    protected:
        GLuint glObject;
    };
}
