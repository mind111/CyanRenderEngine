#include "GfxStaticMesh.h"
#include "GfxContext.h"

namespace Cyan
{
    GfxStaticSubMesh* GfxStaticSubMesh::create(Geometry* geometry)
    {
        return GfxContext::get()->createGfxStaticSubMesh(geometry);
    }

    GfxStaticSubMesh::GfxStaticSubMesh()
    {

    }
}

