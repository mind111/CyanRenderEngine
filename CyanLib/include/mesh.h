#pragma once
#include <string>
#include <map>
#include <array>
#include <glew/glew.h>
#include <mutex>

#include "Asset.h"
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
    struct MeshInstance;

    struct StaticMesh : public Asset
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
            bool bInitialized = false;
        };

        StaticMesh() = default;
        StaticMesh(const char* meshName)
            : Asset(meshName)
        {

        }

        static const char* getClassName() { return "StaticMesh"; }

        /* Asset interface */
        virtual const char* getAssetTypeName() override { return getClassName(); }
        virtual void import() override;
        virtual void load() override { }
        virtual void onLoaded() override;
        virtual void unload() override { }

        static Submesh::Desc getSubmeshDesc(Submesh* submesh);
        Submesh* getSubmesh(u32 index);

        // add a submesh while deferring the rendering data initialization, mainly meant for async loading
        // because this function is called on a thread that doesn't have a valid GL context
        void addSubmeshDeferred(Geometry* inGeometry);
        void onSubmeshAddedDeferred(Submesh* sm);
        // add a submesh while immediate initialize rendering data
        void addSubmeshImmediate(Geometry* inGeometry);
        void onSubmeshAddedImmediate(Submesh* sm);

        void addInstance(MeshInstance* inInstance);
        u32 numSubmeshes();

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

        std::mutex submeshMutex;
        std::vector<std::shared_ptr<Submesh>> submeshes;

        std::mutex instanceMutex;
        std::vector<MeshInstance*> instances;
    };

    struct MeshInstance 
    {
        MeshInstance(StaticMesh* base)
            : mesh(base) 
        {
            mesh->addInstance(this);
            materials.resize(base->numSubmeshes());
        }

        Material* getMaterial(u32 index) 
        {
            std::lock_guard<std::mutex> lock(materialsMutex);
            if (!materials.empty())
            {
                return materials[index];
            }
            return nullptr;
        }

        void addMaterial(Material* matl)
        {
            std::lock_guard<std::mutex> lock(materialsMutex);
            materials.push_back(matl);
        }

        void setMaterial(Material* matl) 
        {
            std::lock_guard<std::mutex> lock(materialsMutex);
            for (u32 i = 0; i < mesh->numSubmeshes(); ++i) 
            {
                setMaterial(matl, i);
            }
        }

        void setMaterial(Material* matl, u32 index) 
        {
            std::lock_guard<std::mutex> lock(materialsMutex);
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

        std::mutex materialsMutex;
        std::vector<Material*> materials;
    };
}

extern float cubeVertices[108];
