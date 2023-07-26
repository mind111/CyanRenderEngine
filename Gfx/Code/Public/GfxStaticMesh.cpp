#include "GfxStaticMesh.h"
#include "GfxContext.h"

namespace Cyan
{
    GfxStaticSubMesh* GfxStaticSubMesh::create(Geometry* geometry)
    {
        // todo: assert running on render thread here
        // todo: detect if we are requesting creating rendering data for the same submesh / geometry here
        return GfxContext::get()->createGfxStaticSubMesh(geometry);
    }

    GfxStaticSubMesh::GfxStaticSubMesh()
    {

    }
}

