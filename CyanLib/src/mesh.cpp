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