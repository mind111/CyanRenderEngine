#pragma once

#include "Gfx.h"
#include "Geometry.h"

namespace Cyan
{
    class GfxStaticSubMesh
    {
    public:
        virtual ~GfxStaticSubMesh() { };
        static GFX_API GfxStaticSubMesh* create(Geometry* geometry);
        virtual void draw() = 0;
    protected:
        GfxStaticSubMesh();
    };
}
