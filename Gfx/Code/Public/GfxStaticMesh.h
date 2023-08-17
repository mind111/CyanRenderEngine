#pragma once

#include "Core.h"
#include "Gfx.h"
#include "Geometry.h"

namespace Cyan
{
    class GfxStaticSubMesh
    {
    public:
        virtual ~GfxStaticSubMesh() { };
        static GFX_API GfxStaticSubMesh* create(std::string subMeshKey, Geometry* geometry);
        virtual void updateVertices(u32 byteOffset, u32 numOfBytes, const void* data) = 0;
        virtual void draw() = 0;
    protected:
        GfxStaticSubMesh();

        static std::unordered_map<std::string, std::unique_ptr<GfxStaticSubMesh>> s_gfxStaticSubMeshMap;
    };
}
