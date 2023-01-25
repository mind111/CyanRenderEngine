#pragma once

#include <glew/glew.h>

#include "Common.h"

#define BINDLESS_TEXTURE 0

namespace Cyan {
    class GpuObject {
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
