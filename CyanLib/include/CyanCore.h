#pragma once

#include <glew/glew.h>

#include "Common.h"

namespace Cyan 
{
    /**
     * GfxResource is basically an encapsulation of graphics API object, in GL's context, such as a vertex buffer object,
        a vertex array object and so on.
     */
    class GfxResource 
    {
    public:
        GfxResource()
            : glObject(-1) 
        { }

        u32 getGpuResource() const { return glObject; }
        // todo: this operator is not robust as object id can get recycled, so same id doesn't guarantee same object
        bool operator==(const GfxResource& rhs) { return glObject == rhs.getGpuResource(); }

    protected:
        GLuint glObject;
    };
}

namespace std
{
    template <>
    struct hash<Cyan::GfxResource>
    {
        std::size_t operator()(const Cyan::GfxResource& gpuResource) const
        {
            return std::hash<GLuint>()(gpuResource.getGpuResource());
        }
    };
}
