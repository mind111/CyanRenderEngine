#pragma once
#include <string>
#include <map>
#include <array>
#include <glew/glew.h>

#include "MathUtils.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "Material.h"
#include "Transform.h"
#include "BVH.h"
#include "Asset.h"
#include "ShaderStorageBuffer.h"
#include "Geometry.h"

namespace Cyan
{
    struct Geometry;

    struct StaticMesh
    {
        struct Submesh
        {
            struct Desc
            {
                u32 type;
                u32 vertexOffset;
                u32 numVertices;
                u32 indexOffset;
                u32 numIndices;
                u32 padding;
            };

            Submesh() = default;
            Submesh(Geometry* inGeometry);
            ~Submesh() { }

            void init();

            u32 numVertices() const { return geometry->numVertices(); }
            u32 numIndices() const { return geometry->numIndices(); }

            Geometry* geometry = nullptr;
            VertexArray* va = nullptr;
            i32 index = -1;
        };

        StaticMesh() = default;
        StaticMesh(const char* meshName)
            : name(meshName)
        {

        }

        static Submesh::Desc getSubmeshDesc(const Submesh& submesh);
        Submesh getSubmesh(u32 index) { return submeshes[index]; }
        void addSubmesh(Geometry* inGeometry);
        const BoundingBox3D& getAABB() { return aabb; }
        u32 numSubmeshes() { return submeshes.size(); }

        using SubmeshBuffer = ShaderStorageBuffer<DynamicSsboData<Submesh::Desc>>;
        using TriVertexBuffer = ShaderStorageBuffer<DynamicSsboData<Triangles::Vertex>>;
        using TriIndexBuffer = ShaderStorageBuffer<DynamicSsboData<u32>>;
        using PointVertexBuffer = ShaderStorageBuffer<DynamicSsboData<PointCloud::Vertex>>;
        using PointIndexBuffer = ShaderStorageBuffer<DynamicSsboData<u32>>;
        using LineVertexBuffer = ShaderStorageBuffer<DynamicSsboData<Lines::Vertex>>;
        using LineIndexBuffer = ShaderStorageBuffer<DynamicSsboData<u32>>;

        static SubmeshBuffer& getSubmeshBuffer();
        static TriVertexBuffer& getTriVertexBuffer();
        static TriIndexBuffer& getTriIndexBuffer();

        std::string name;
        BoundingBox3D aabb;
        std::vector<Submesh> submeshes;
    };

    struct MeshInstance 
    {
        MeshInstance(StaticMesh* base)
            : mesh(base) 
        {
            materials.resize(base->numSubmeshes());
        }

        Material* getMaterial(u32 index) 
        {
            return materials[index];
        }

        void setMaterial(Material* matl) 
        {
            for (u32 i = 0; i < mesh->numSubmeshes(); ++i) 
            {
                setMaterial(matl, i);
            }
        }

        void setMaterial(Material* matl, u32 index) 
        {
            materials[index] = matl;
        }

        BoundingBox3D getAABB(const glm::mat4& worldTransform) 
        {
            BoundingBox3D parentAABB = mesh->getAABB();
            BoundingBox3D aabb;
            aabb.bound(worldTransform * parentAABB.pmin);
            aabb.bound(worldTransform * parentAABB.pmax);
            return aabb;
        }

        StaticMesh* mesh = nullptr;
        std::vector<Material*> materials;
    };
}

extern float cubeVertices[108];
