#pragma once

#include "Asset.h"
#include "Gfx.h"
#include "Geometry.h"
#include "GfxStaticMesh.h"
#include "Transform.h"

namespace Cyan
{
    // class Material;
    // class MaterialInstance;
    class StaticMeshInstance;

    class GFX_API StaticMesh : public Asset
    {
    public:
        friend class Scene;
        friend class SceneRenderer;

        struct SubMesh
        {
            SubMesh(StaticMesh* inParent, std::unique_ptr<Geometry> inGeometry);
            ~SubMesh();

            i32 numVertices() { return geometry->numVertices(); }
            i32 numIndices() { return geometry->numIndices(); }

            // raw data
            StaticMesh* parent = nullptr;
            std::unique_ptr<Geometry> geometry = nullptr;

            void createGfxProxy();

            // gfx data
            GfxStaticSubMesh* gfxProxy = nullptr;
        };

        StaticMesh(const char* name, u32 numSubMeshes);
        ~StaticMesh();

        SubMesh* operator[](u32 index) 
        { 
            assert(index < m_numSubMeshes);
            return m_subMeshes[index].get();
        }

        SubMesh* operator[](i32 index) 
        { 
            assert(static_cast<u32>(index) < m_numSubMeshes);
            return m_subMeshes[index].get();
        }

        std::unique_ptr<StaticMeshInstance> createInstance(const Transform& localToWorld);
        // todo: implement this
        void removeInstance();

        u32 numSubMeshes() const { return m_numSubMeshes; }
        void setSubMesh(std::unique_ptr<SubMesh> sm, u32 slot);
    private:
        const u32 m_numSubMeshes;
        std::vector<std::unique_ptr<SubMesh>> m_subMeshes;
        std::vector<StaticMeshInstance*> m_instances;
    };

    class GFX_API StaticMeshInstance
    {
    public:
        StaticMeshInstance(StaticMesh* parent, const Transform& localToWorld);
        ~StaticMeshInstance();

        StaticMesh* getParentMesh() { return m_parent; }
        const Transform& getLocalToWorldTransform() { return m_localToWorldTransform; }
        const glm::mat4& getLocalToWorldMatrix() { return m_localToWorldMatrix; }
        void setLocalToWorldTransform(const Transform& localToWorld);
        // void setMaterial(Material);
    private:
        StaticMesh* m_parent = nullptr;
        Transform m_localToWorldTransform;
        glm::mat4 m_localToWorldMatrix;
        // std::vector<MaterialInstance*> materials;
    };
}