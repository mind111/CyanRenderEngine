#pragma once
#include <string>
#include <map>
#include <array>
#include <glew/glew.h>

#include "MathUtils.h"
#include "Material.h"
#include "BVH.h"
#include "ShaderStorageBuffer.h"
#include "Geometry.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"

namespace Cyan
{
    struct Geometry;

    struct StaticMesh
    {
        struct Submesh
        {
            struct Desc
            {
                u32 vertexOffset;
                u32 numVertices;
                u32 indexOffset;
                u32 numIndices;
            };

            Submesh() = default;
            Submesh(Geometry* inGeometry);
            ~Submesh() { }

            void init();

            u32 numVertices() const { return geometry->numVertices(); }
            u32 numIndices() const { return geometry->numIndices(); }
            VertexArray* getVertexArray() { return va.get(); }

            Geometry* geometry = nullptr;
            std::shared_ptr<VertexBuffer> vb = nullptr;
            std::shared_ptr<IndexBuffer> ib = nullptr;
            std::shared_ptr<VertexArray> va = nullptr;
            i32 index = -1;
        };

        StaticMesh() = default;
        StaticMesh(const char* meshName)
            : name(meshName)
        {

        }

        static Submesh::Desc getSubmeshDesc(Submesh* submesh);
        Submesh* getSubmesh(u32 index) { return &submeshes[index]; }
        void addSubmesh(Geometry* inGeometry);
        u32 numSubmeshes() { return submeshes.size(); }

        const BoundingBox3D& getAABB() { return aabb; }

        struct Vertex
        {
            glm::vec4 pos;
            glm::vec4 normal;
            glm::vec4 tangent;
            glm::vec4 texCoord;
        };

        using SubmeshBuffer = ShaderStorageBuffer<DynamicSsboData<Submesh::Desc>>;
        using GlobalVertexBuffer = ShaderStorageBuffer<DynamicSsboData<Vertex>>;
        using GlobalIndexBuffer = ShaderStorageBuffer<DynamicSsboData<u32>>;

        static SubmeshBuffer& getSubmeshBuffer();
        static GlobalVertexBuffer& getGlobalVertexBuffer();
        static GlobalIndexBuffer& getGlobalIndexBuffer();

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
