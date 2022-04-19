#pragma once
#include <string>
#include <map>
#include "glew.h"

#include "MathUtils.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "Material.h"
#include "Transform.h"
#include "Geometry.h"
#include "BVH.h"

struct Scene;

namespace Cyan
{
    template <typename GeomType>
    struct Mesh
    {
        template <typename GeomType>
        struct Submesh
        {
            GeomType geometry;
            VertexArray* va;
        };

        std::string name;
        std::vector<Submesh<GeomType>> submeshes;
    };

    template <typename GeomType, typename MaterialType>
    struct Renderable
    {
        Mesh::Submesh<GeomType>* mesh;
        MaterialInstance<MaterialType>* matl;
    };

    template <typename GeomType, typename MaterialType>
    struct MeshInstance
    {
        struct Renderable
        {
            Mesh::Submesh<GeomType>* submesh;
            MaterialInstance<MaterialType> matl;
        };

        std::vector<Renderable> renderables;
    };

    template <typename GeomType, typename MaterialType>
    struct SceneComponent
    {
        MeshInstance<GeomType, MaterialType>* meshInstance;
    }

    template <typename GeomType, typename MaterialType>
    void createSceneNode(Mesh<GeomType> mesh, )
    {

    }

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

    struct QuadMesh
    {
        struct Vertex
        {
            glm::vec3 position;
            glm::vec2 texCoords;
        };

        QuadMesh() {};
        QuadMesh(glm::vec2 center, glm::vec2 extent);
        ~QuadMesh() {}

        void init(glm::vec2 center, glm::vec2 extent);

        glm::vec2 m_center;
        glm::vec2 m_extent;
        Vertex m_verts[6];
        VertexArray* m_vertexArray;
    };
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
