#include "Common.h"
#include "CyanAPI.h"
#include "Mesh.h"
#include "LightMap.h"
#include "Scene.h"
#include "Asset.h"

namespace Cyan
{
    std::string Mesh::typeDesc = std::string("Mesh");

    /*
    * calculate mesh's aabb on initialization
    */
    void Mesh::init()
    {
        for (u32 i = 0; i < numSubmeshes(); ++i)
        {
            ISubmesh* sm = submeshes[i];
            aabb.bound(submeshes[i]->getMin());
            aabb.bound(submeshes[i]->getMax());
        }
    }

#if 0
    void Mesh::onFinishLoading()
    {
        Toolkit::GpuTimer timer("Mesh::onFinishLoading()", false);
        if (m_shouldNormalize)
            m_normalization = Toolkit::computeMeshNormalization(this);
        else
            Toolkit::computeAABB(this);
#ifdef _DEBUG
        cyanInfo("%s mesh has %u triangles", m_name.c_str(), numTriangles());
#endif
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

    u32 Mesh::numSubMeshes()
    {
        return (u32)m_subMeshes.size();
    }

    u32 Mesh::numTriangles()
    {
        u32 numTriangles = 0;
        for (u32 sm = 0; sm < m_subMeshes.size(); ++sm)
            numTriangles += (m_subMeshes[sm]->m_numIndices / 3);
        return numTriangles;
    }

    void MeshInstance::setMaterial(u32 index, MaterialInstance* matl)
    {
        m_matls[index] = matl;
    }

    BoundingBox3D& MeshInstance::getAABB()
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
        vb->addVertexAttrib({VertexAttrib::DataType::Float, 3, sizeof(Vertex), 0u });
        vb->addVertexAttrib({VertexAttrib::DataType::Float, 2, sizeof(Vertex), sizeof(glm::vec3) });
        m_vertexArray = createVertexArray(vb);
        // TODO: refactor how vb interacts with va, vb->finalize() va->onVbFinalize();
        m_vertexArray->init();
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