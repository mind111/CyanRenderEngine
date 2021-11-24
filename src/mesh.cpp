#include "Common.h"
#include "CyanAPI.h"
#include "Mesh.h"

namespace Cyan
{
    void Mesh::onFinishLoading()
    {
        Toolkit::ScopedTimer timer("Mesh::onFinishLoading()", false);
        if (m_shouldNormalize)
        {
            m_normalization = Toolkit::computeMeshNormalization(this);
        }
        else
        {
            Toolkit::computeAABB(this);
        }
        timer.end();
    }

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
                    sm->m_triangles.m_positionArray[j * 3],
                    sm->m_triangles.m_positionArray[j * 3 + 1],
                    sm->m_triangles.m_positionArray[j * 3 + 2]
                };

                float currentHit = tri.intersectRay(objectSpaceRo, objectSpaceRd); 
                if (currentHit > 0.f && currentHit < globalHit.t)
                {
                    // compute face normal for skipping backfaced triangles
                    auto v0 = tri.m_vertices[0]; 
                    auto v1 = tri.m_vertices[1]; 
                    auto v2 = tri.m_vertices[2];

                    glm::vec3 faceNormal = glm::cross(v1 - v0, v2 - v0);
                    if (glm::dot(objectSpaceRd, faceNormal) < 0.f)
                    {
                        globalHit.smIndex = i;
                        globalHit.triangleIndex = j;
                        globalHit.t = currentHit;
                    }
                }
            }
        }
        return globalHit;
    }

    bool Mesh::bruteForceVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {

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
    bool Mesh::intersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
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

    MeshInstance* Mesh::createInstance()
    {
        MeshInstance* instance = (MeshInstance*)CYAN_ALLOC(sizeof(MeshInstance));
        instance->m_mesh = this;
        u32 numSubMeshes = (u32)this->m_subMeshes.size();
        instance->m_matls = (MaterialInstance**)CYAN_ALLOC(sizeof(MaterialInstance*) * numSubMeshes);
        return instance;
    }

    u32 Mesh::numSubMeshes()
    {
        return (u32)m_subMeshes.size();
    }

    void MeshInstance::setMaterial(u32 index, MaterialInstance* matl)
    {
        m_matls[index] = matl;
    }

    BoundingBox3f& MeshInstance::getAABB()
    {
        return m_mesh->m_aabb;
    }

    QuadMesh::QuadMesh(glm::vec2 center, glm::vec2 extent)
        : m_center(center), m_extent(extent), m_vertexArray(0)
    {
    }
    void QuadMesh::init(glm::vec2 center, glm::vec2 extent)
    {
        m_center = center;
        m_extent = extent;

        m_verts[0].position = glm::vec3(m_center + glm::vec2(-1.f,  1.f) * m_extent, 0.f);
        m_verts[1].position = glm::vec3(m_center + glm::vec2(-1.0f, -1.0f) * m_extent, 0.f); 
        m_verts[2].position = glm::vec3(m_center + glm::vec2( 1.0f, -1.0f) * m_extent, 0.f);
        m_verts[3].position = glm::vec3(m_center + glm::vec2(-1.0f,  1.0f) * m_extent, 0.f);
        m_verts[4].position = glm::vec3(m_center + glm::vec2( 1.0f, -1.0f) * m_extent, 0.f);
        m_verts[5].position = glm::vec3(m_center + glm::vec2( 1.0f,  1.0f) * m_extent, 0.f);

        m_verts[0].texCoords = glm::vec2(0.f, 1.f);
        m_verts[1].texCoords = glm::vec2(0.f, 0.f); 
        m_verts[2].texCoords = glm::vec2(1.0f, 0.0f);
        m_verts[3].texCoords = glm::vec2(0.f, 1.f);
        m_verts[4].texCoords = glm::vec2(1.f, 0.f);
        m_verts[5].texCoords = glm::vec2(1.f, 1.f);

        auto vb = createVertexBuffer(m_verts, 6u * sizeof(Vertex), sizeof(Vertex), 6u);
        vb->addVertexAttrb({VertexAttrib::DataType::Float, 3, sizeof(Vertex), 0u, 0});
        vb->addVertexAttrb({VertexAttrib::DataType::Float, 2, sizeof(Vertex), sizeof(glm::vec3), 0});
        m_vertexArray = createVertexArray(vb);
        // TODO: refactor how vb interacts with va, vb->finalize() va->onVbFinalize();
        m_vertexArray->init();
    }
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