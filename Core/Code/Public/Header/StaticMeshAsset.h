#include "Asset.h"

#include "Core.h"
#include "Geometry.h"

namespace Cyan
{
    class CORE_API StaticMeshAsset : public Asset
    {
    public:
        struct Submesh
        {
            Submesh(StaticMeshAsset* inOwner, i32 submeshIndex, std::unique_ptr<Geometry> inGeometry);
            ~Submesh() { }

            i32 numVertices() { return geometry->numVertices(); }
            i32 numIndices() { return geometry->numIndices(); }

            StaticMeshAsset* owner = nullptr;
            i32 index = -1; // index into parent's submesh array
            std::unique_ptr<Geometry> geometry = nullptr;
        };

        StaticMeshAsset(const char* name);
        ~StaticMeshAsset() { }
    };
}