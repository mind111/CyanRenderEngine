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
        static GFX_API GfxStaticSubMesh* find(const std::string& subMeshKey);
        virtual void updateVertices(u32 byteOffset, u32 numOfBytes, const void* data) = 0;
        virtual void draw() = 0;

        glm::vec3 m_min = glm::vec3(-FLT_MAX);
        glm::vec3 m_max = glm::vec3(FLT_MAX);
    protected:
        GfxStaticSubMesh();

        static std::unordered_map<std::string, std::unique_ptr<GfxStaticSubMesh>> s_gfxStaticSubMeshMap;
    };
}
