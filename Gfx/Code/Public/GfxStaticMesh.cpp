#include "GfxStaticMesh.h"
#include "GfxContext.h"
#include "GfxModule.h"

namespace Cyan
{
    std::unordered_map<std::string, std::unique_ptr<GfxStaticSubMesh>> GfxStaticSubMesh::s_gfxStaticSubMeshMap;

    GfxStaticSubMesh* GfxStaticSubMesh::create(std::string subMeshKey, Geometry* geometry)
    {
        assert(GfxModule::isInRenderThread());
        auto entry = s_gfxStaticSubMeshMap.find(subMeshKey);
        if (entry != s_gfxStaticSubMeshMap.end())
        {
            return entry->second.get();
        }

        cyanInfo("Creating new GfxStaticSubMesh for %s", subMeshKey.c_str());
        std::unique_ptr<GfxStaticSubMesh> newSubMesh = GfxContext::get()->createGfxStaticSubMesh(geometry);
        s_gfxStaticSubMeshMap.insert({ subMeshKey, std::move(newSubMesh) });
        return s_gfxStaticSubMeshMap[subMeshKey].get();
    }

    GfxStaticSubMesh::GfxStaticSubMesh()
    
    {

    }
}

