#include "StaticMeshAsset.h"

namespace Cyan
{
    StaticMeshAsset::StaticMeshAsset(const char* name)
        : Asset(name)
    {

    }

    StaticMeshAsset::Submesh::Submesh(StaticMeshAsset* inOwner, i32 submeshIndex, std::unique_ptr<Geometry> inGeometry)
        : owner(inOwner), index(submeshIndex), geometry(std::move(inGeometry))
    {
    }
}
