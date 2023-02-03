#include "Common.h"
#include "Mesh.h"
#include "Geometry.h"

namespace Cyan
{
    StaticMesh::Submesh::Submesh(Geometry* inGeometry)
        : geometry(inGeometry), va(nullptr), index(-1)
    {
        Desc desc = { };
        Geometry::Type type = geometry->getGeometryType();
        switch (type)
        {
        case Geometry::Type::kTriangles:
            desc.type = (u32)Geometry::Type::kTriangles;
            desc.vertexOffset = getTriVertexBuffer().getNumElements();
            desc.numVertices = geometry->numVertices();
            desc.indexOffset = getTriIndexBuffer().getNumElements();
            desc.numIndices = geometry->numIndices();
            break;
        case Geometry::Type::kPointCloud:
        case Geometry::Type::kLines:
        default:
            assert(0);
        }

        auto& submeshBuffer = getSubmeshBuffer();
        index = submeshBuffer.getNumElements();
        submeshBuffer.addElement(desc);
    }


    StaticMesh::SubmeshBuffer& StaticMesh::getSubmeshBuffer()
    {
        static SubmeshBuffer s_submeshBuffer("SubmeshBuffer");
        return s_submeshBuffer;
    }

    StaticMesh::TriVertexBuffer& StaticMesh::getTriVertexBuffer()
    {
        static TriVertexBuffer s_triVertexBuffer("TriVertexBuffer");
        return s_triVertexBuffer;
    }

    StaticMesh::TriIndexBuffer& StaticMesh::getTriIndexBuffer()
    {
        static TriIndexBuffer s_triIndexBuffer("TriIndexBuffer");
        return s_triIndexBuffer;
    }

    StaticMesh::Submesh::Desc StaticMesh::getSubmeshDesc(const StaticMesh::Submesh& submesh)
    {
        return getSubmeshBuffer()[submesh.index];
    }

    // todo: implement this  
    void StaticMesh::Submesh::init()
    {
        va = new VertexArray();
    }

    void StaticMesh::addSubmesh(Geometry* inGeometry)
    {
        submeshes.emplace_back(inGeometry);
    }

#if 0
    MeshRayHit Mesh::bruteForceIntersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        MeshRayHit globalHit;

        for (u32 i = 0; i < m_subMeshes.size(); ++i)
        {
            auto sm = m_subMeshes[i];
            u32 numTriangles = sm->m_triangles.m_numVerts / 3;
            for (u32 j = 0; j < numTriangles; ++j)
            {
                Triangle tri = {
                    sm->m_triangles.positions[j * 3],
                    sm->m_triangles.positions[j * 3 + 1],
                    sm->m_triangles.positions[j * 3 + 2]
                };

                float currentHit = tri.intersectRay(objectSpaceRo, objectSpaceRd); 
                if (currentHit > 0.f && currentHit < globalHit.t)
                {
                    globalHit.smIndex = i;
                    globalHit.triangleIndex = j;
                    globalHit.t = currentHit;
                    globalHit.mesh = this;
                }
            }
        }

        return globalHit;
    }

    bool Mesh::bruteForceVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        for (u32 i = 0; i < m_subMeshes.size(); ++i)
        {
            auto sm = m_subMeshes[i];
            u32 numTriangles = sm->m_triangles.m_numVerts / 3;
            for (u32 j = 0; j < numTriangles; ++j)
            {
                Triangle tri = {
                    sm->m_triangles.positions[j * 3],
                    sm->m_triangles.positions[j * 3 + 1],
                    sm->m_triangles.positions[j * 3 + 2]
                };

                float currentHit = tri.intersectRay(objectSpaceRo, objectSpaceRd); 
                if (currentHit > 0.f)
                    return true;
            }
        }
        return false;
    }

    bool Mesh::bvhVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        auto bvhRoot = m_bvh->root;
        return bvhRoot->traceVisibility(objectSpaceRo, objectSpaceRd);
    }

    MeshRayHit Mesh::bvhIntersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        return m_bvh->trace(objectSpaceRo, objectSpaceRd);
    }

    MeshRayHit Mesh::intersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        MeshRayHit meshRayHit;
        if (m_bvh)
            meshRayHit = bvhIntersectRay(objectSpaceRo, objectSpaceRd);
        else
            meshRayHit = bruteForceIntersectRay(objectSpaceRo, objectSpaceRd);
        if (meshRayHit.smIndex < 0 || meshRayHit.triangleIndex < 0)
            meshRayHit.t = -1.f;
        return meshRayHit; 
    }

    // coarse binary ray test
    bool Mesh::castVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        if (m_bvh)
            return bvhVisibilityRay(objectSpaceRo, objectSpaceRd);
        else
            return bruteForceVisibilityRay(objectSpaceRo, objectSpaceRd);
    }

    MeshInstance* Mesh::createInstance(Scene* scene)
    {
        MeshInstance* instance = new MeshInstance;
        instance->m_mesh = this;
        u32 numSubMeshes = (u32)this->m_subMeshes.size();
        instance->m_matls = (MaterialInstance**)CYAN_ALLOC(sizeof(MaterialInstance*) * numSubMeshes);
        instance->m_lightMap = nullptr;
#if 1
        // apply obj material defined in the asset
        if (!m_objMaterials.empty())
        {
            for (u32 sm = 0; sm < numSubMeshes; ++sm)
            {
                i32 matlIdx = m_subMeshes[sm]->m_materialIdx;
                if (matlIdx >= 0)
                {
                    auto& objMatl = m_objMaterials[matlIdx];
                    PbrMaterialParam params = { };
                    params.flatBaseColor = glm::vec4(objMatl.diffuse, 1.f);
                    // hard-coded default roughness/metallic for obj materials
                    params.kMetallic = 0.02f;
                    params.kRoughness = 0.5f;
                    params.hasBakedLighting = 0.f;
                    params.indirectDiffuseScale = 1.f;
                    params.indirectSpecularScale = 1.f;
                    auto matl = new StandardPbrMaterial(params);
                    instance->m_matls[sm] = matl->m_materialInstance;
                    scene->addStandardPbrMaterial(matl);
                }
            }
        }
#endif
        return instance;
    }
#endif
};

float cubeVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};