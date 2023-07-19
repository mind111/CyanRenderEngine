#pragma once

#include "Asset.h"
#include "Gfx.h"
#include "Geometry.h"

namespace Cyan
{
    class VertexBuffer;
    class IndexBuffer;
    class Material;
    class MaterialInstance;
    class StaticMeshInstance;

    class GFX_API StaticMesh : public Asset
    {
    public:
        friend class Scene;
        friend class SceneRenderer;

        struct SubMesh
        {
            SubMesh(StaticMesh* inParent, std::unique_ptr<Geometry> inGeometry);
            ~SubMesh() { }

            i32 numVertices() { return geometry->numVertices(); }
            i32 numIndices() { return geometry->numIndices(); }

            // raw data
            StaticMesh* parent = nullptr;
            std::unique_ptr<Geometry> geometry = nullptr;

            void initGfxData();

            // gfx data
            bool bReadyForRendering = false;
            std::unique_ptr<VertexBuffer> vb = nullptr;
            std::unique_ptr<IndexBuffer> ib = nullptr;
        };

        StaticMesh(const char* name, u32 numSubMeshes);
        ~StaticMesh() { }

        StaticMeshInstance* createInstance();
        void removeInstance();

        u32 numSubMeshes() const { return m_numSubMeshes; }
        void setSubMesh(std::unique_ptr<SubMesh> sm, u32 slot);
    private:
        const u32 m_numSubMeshes;
        std::vector<std::unique_ptr<SubMesh>> m_subMeshes;
        std::vector<StaticMeshInstance*> m_instances;
    };

    class StaticMeshInstance
    {
    public:
        StaticMeshInstance(StaticMesh* inParent, const glm::mat4& inTransform);
        ~StaticMeshInstance() { }

    private:
        StaticMesh* parent = nullptr;
        glm::mat4 transform;
        std::vector<MaterialInstance*> materials;
    };
}