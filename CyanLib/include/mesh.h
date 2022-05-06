#pragma once
#include <string>
#include <map>
#include <array>
#include "glew.h"

#include "MathUtils.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "Material.h"
#include "Transform.h"
#include "Geometry.h"
#include "BVH.h"
#include "AssetFactory.h"

struct Scene;

namespace Cyan
{
    // submesh interface
    struct BaseSubmesh
    {
        virtual u32 numVertices() = 0;
        virtual u32 numIndices() = 0;
        virtual glm::vec3 getMin() = 0;
        virtual glm::vec3 getMax() = 0;
        virtual VertexArray* getVertexArray() = 0;
    };

    struct Mesh : public Asset
    {
        template <typename Geometry>
        struct Submesh : public BaseSubmesh
        {
            Submesh(const std::vector<typename Geometry::Vertex>& vertices, const std::vector<u32>& indices)
            {
                geometry.vertices = std::move(vertices);
                geometry.indices = std::move(indices);

                init();
            }

            Submesh(const Geometry& srcGeom)
            {
                // todo: transfer geometry data

                init();
            }

            virtual u32 numVertices() override { return geometry.numVertices(); }
            virtual u32 numIndices() override { return geometry.numIndices(); }
            virtual VertexArray* getVertexArray() { return va; }
            const typename std::vector<typename Geometry::Vertex>::iterator& getVertices() { return geometry.vertices.begin(); }
            const std::vector<u32>::iterator& getIndices() { return geometry.indices.begin(); }
            void setGeometryData(std::vector<typename Geometry::Vertex>& vertices, const std::vector<u32>& indices)
            {
                geometry.vertices = vertices;
                geometry.indices = indices;

                // release old resources if it exists
                if (va)
                {
                    va->release();
                    delete va;
                }

                // update gpu data
                init();
            }

            virtual glm::vec3 getMin() override { return pmin; }
            virtual glm::vec3 getMax() override { return pmax; }

        private:
            /*
            * init vertex buffer and vertex array
            */
            void init()
            {
                VertexSpec vertexSpec;

                u8 flags = Geometry::Vertex::getFlags();
                if (flags && (u8)VertexAttribFlags::kHasPosition)
                {
                    vertexSpec.addAttribute({ "POSITION", 3, 0 });
                }
                if (flags && (u8)VertexAttribFlags::kHasNormal)
                {
                    vertexSpec.addAttribute({ "NORMAL", 3, 0 });
                }
                if (flags && (u8)VertexAttribFlags::kHasTangent)
                {
                    vertexSpec.addAttribute({ "TANGENT", 3, 0 });
                }
                if (flags && (u8)VertexAttribFlags::kHasTexCoord0)
                {
                    vertexSpec.addAttribute({ "TEXCOORD0", 3, 0 });
                }
                if (flags && (u8)VertexAttribFlags::kHasTexCoord1)
                {
                    vertexSpec.addAttribute({ "TEXCOORD1", 3, 0 });
                }
                auto vb = createVertexBuffer(geometry.vertices.data(), sizeof(Geometry::Vertex) * geometry.vertices.size(), std::move(vertexSpec));
                va = createVertexArray(vb, &geometry.indices);

                for (u32 i = 0; i < numVertices(); ++i)
                {
                    pmin.x = min(pmin.x, geometry.vertices[i].pos.x);
                    pmin.y = min(pmin.y, geometry.vertices[i].pos.y);
                    pmin.z = min(pmin.z, geometry.vertices[i].pos.z);

                    pmax.x = max(pmax.x, geometry.vertices[i].pos.x);
                    pmax.y = max(pmax.y, geometry.vertices[i].pos.y);
                    pmax.z = max(pmax.z, geometry.vertices[i].pos.z);
                }
            }

            Geometry geometry;
            VertexArray* va;
            glm::vec3 pmin = glm::vec3(FLT_MAX);
            glm::vec3 pmax = glm::vec3(-FLT_MAX);
        };

        Mesh() = default;
        Mesh(const char* meshName, const std::vector<BaseSubmesh*>& srcSubmeshes)
            : name(meshName) 
        { 
            submeshes = std::move(srcSubmeshes);
        }

        virtual std::string getAssetObjectTypeDesc() override { return typeDesc; }
        static std::string getAssetClassTypeDesc() { return typeDesc; }
        const BoundingBox3D& getAABB() { return aabb; }
        u32 numSubmeshes() { return submeshes.size(); }
        void init();
        
        static std::string typeDesc;
        std::string name;
        BoundingBox3D aabb;
        std::vector<BaseSubmesh*> submeshes;
    };

    struct MeshInstance
    {
        Mesh* parent = nullptr;
        std::vector<BaseMaterial*> materials;

        MeshInstance(Mesh* base)
            : parent(base)
        {
            materials.resize(base->numSubmeshes());
        }

        template <typename T>
        T* getMaterial(u32 index)
        {
            // type check
            if (T::getAssetClassTypeDesc() != materials[index]->getAssetObjectTypeDesc())
            {
                return nullptr;
            }
            return static_cast<T*>(materials[index]);
        }

        void setMaterial(BaseMaterial* matl, u32 index)
        {
            materials[index] = matl;
        }

        BoundingBox3D getAABB(const glm::mat4& worldTransform)
        {
            BoundingBox3D parentAABB = parent->getAABB();
            BoundingBox3D aabb;
            aabb.bound(worldTransform * parentAABB.pmin);
            aabb.bound(worldTransform * parentAABB.pmax);
            return aabb;
        }
    };

    struct RayTracingMesh
    {
        TriangleArray triangles;
        BVH
    };

#if 0
    struct MeshInstance;

    struct MeshRayHit
    {
        struct Mesh* mesh;
        f32   t;
        i32   smIndex;
        i32   triangleIndex;

        MeshRayHit()
            : mesh(nullptr), t(FLT_MAX), smIndex(-1), triangleIndex(-1)
        {}
    };

    // todo: split ray tracing required geometry data from this class
    struct Mesh
    {
        void onFinishLoading();
        MeshInstance* createInstance(Scene* scene);
        u32 numSubMeshes();
        u32 numTriangles();
        MeshRayHit bruteForceIntersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd);
        MeshRayHit bvhIntersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd);
        MeshRayHit intersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd);
        bool       bruteForceVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd);
        bool       bvhVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd);
        bool       castVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd);
        Triangle getTriangle(u32 smIndex, u32 triIndex) {
            const auto& sm = m_subMeshes[smIndex];
            u32 offset = triIndex * 3;
            return {
                sm->m_triangles.positions[offset],
                sm->m_triangles.positions[offset + 1],
                sm->m_triangles.positions[offset + 2]
            };
        }

        struct SubMesh
        {
            VertexArray*     m_vertexArray;
            u32              m_numVerts;
            u32              m_numIndices;
            i32              m_materialIdx;
            TriangleArray    m_triangles;
        };

        i32         m_lightMapWidth;
        i32         m_lightMapHeight;
        std::string m_name;
        bool        m_shouldNormalize;
        glm::mat4 m_normalization;
        std::vector<SubMesh*> m_subMeshes;
        std::vector<ObjMaterial> m_objMaterials;
        BoundingBox3D m_aabb;
        MeshBVH* m_bvh;
    };

    struct MeshInstance
    {
        MaterialInstance* getMaterial(u32 index)
        {
            return m_matls[index];
        }
        void setMaterial(u32 index, MaterialInstance* matl);

        Mesh* m_mesh;
        MaterialInstance** m_matls;
        std::vector<u32>   m_rtMatls;
        BoundingBox3D& getAABB();
        struct LightMap* m_lightMap;
    };
#endif
}

// TODO: implement serialization
struct MeshBinaryDescriptor
{
    u32 version;
    u32 name;
    u32 shouldNormalize;
    u32 numSubMeshes;
};

extern float cubeVertices[108];
