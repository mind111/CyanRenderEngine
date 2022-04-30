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

    };

    // todo: should we allow mesh that contains different type of submeshes ...?
    struct Mesh : public Asset
    {
        template <typename Geometry>
        struct Submesh : public BaseSubmesh
        {
            Submesh(const std::vector<Geometry::Vertex>& vertices, const std::vector<u32>& indices)
            {
                geometry.vertices = std::move(vertices);
                geometry.indices = std::move(indices);

                init();
            }

            Submesh(const Geometry& srcGeom)
            {
                // transfer geometry data

                init();
            }

            u32 numVertices() { return geometry.numVertices(); }
            u32 numIndices() { return geometry.numIndices(); }

        private:
            /*
            * init vertex buffer and vertex array
            */
            void init()
            {

            }

            Geometry geometry;
            VertexArray* va;
        };

        Mesh() = default;
        Mesh(const char* meshName, const std::vector<BaseSubmesh*>& srcSubmeshes)
            : name(meshName) 
        { 
            submeshes = std::move(srcSubmeshes);
        }

        virtual std::string getAssetTypeIdentifier() override { return typeIdentifier; }
        static std::string getAssetTypeIdentifier() { return typeIdentifier; }

        u32 numSubmeshes() { return submeshes.size(); }

        std::string name;
        std::vector<BaseSubmesh*> submeshes;
    };

    struct MeshInstance
    {
        Mesh* mesh = nullptr;
        std::vector<BaseMaterial*> materials;

        MeshInstance(Mesh* base)
            : mesh(base)
        {
            materials.resize(base->numSubmeshes());
        }

        void setMaterial(BaseMaterial* matl, u32 index)
        {
            materials[index] = matl;
        }
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
                sm->m_triangles.m_positionArray[offset],
                sm->m_triangles.m_positionArray[offset + 1],
                sm->m_triangles.m_positionArray[offset + 2]
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
        BoundingBox3f m_aabb;
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
        BoundingBox3f& getAABB();
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
