#include "BVH.h"
#include "Mesh.h"
#include "CyanAPI.h"

namespace Cyan
{
    MeshBVH::MeshBVH(Mesh* mesh) 
        : m_owner(mesh), root(nullptr), numNodes(0)
    { 
        u32 totalNumTriangles = 0u;
        for (u32 sm = 0; sm < mesh->m_subMeshes.size(); ++sm)
        {
            auto subMesh = mesh->m_subMeshes[sm];
            totalNumTriangles += subMesh->m_triangles.m_numVerts / 3;
        }
        // will this overflow because number of triangles is too big ...?
        u32 maxNumNodes = 2u * totalNumTriangles - 1;
        nodes.resize(maxNumNodes);
        leafNodes.resize(totalNumTriangles);

        // initialize leaf nodes
        u32 triangleCounter = 0u;
        for (u32 sm = 0; sm < mesh->m_subMeshes.size(); ++sm)
        {
            auto subMesh = mesh->m_subMeshes[sm];
            u32 numTriangles = subMesh->m_triangles.m_numVerts / 3;
            for (u32 f = 0; f < numTriangles; ++f)
            {
                leafNodes[triangleCounter].bound(mesh->getTriangle(sm, f));
                leafNodes[triangleCounter].m_submeshIndex = sm;
                leafNodes[triangleCounter++].m_triangleIndex = f;
            }
        }
    }

    BVHNode* MeshBVH::allocNode()
    {
        CYAN_ASSERT(numNodes < nodes.size(), "Too many BVHNode allocated! \n");
        return &nodes[numNodes++];
    }

    void MeshBVH::onBuildFinished()
    {
        // shrink the node array
        nodes.resize(numNodes);
    }

    // TODO: improve the tree by not choosing the split axis randomly 
    void MeshBVH::build()
    {
        Toolkit::ScopedTimer timer("MeshBVH::build()", true);
        cyanInfo("Building bvh for %s mesh", m_owner->m_name.c_str());
        root = allocNode(); 

        // fix rand() seed
        srand(1);
        recursiveBuild(0, (u32)leafNodes.size(), root);
    }

    i32 sortAlongX(const void* a,  const void* b)
    {
        auto nodeA = static_cast<const BVHNode*>(a);
        auto nodeB = static_cast<const BVHNode*>(b);
        f32 centerA = (nodeA->m_aabb.m_pMin.x + nodeA->m_aabb.m_pMax.x) * .5f;
        f32 centerB = (nodeB->m_aabb.m_pMin.x + nodeB->m_aabb.m_pMax.x) * .5f;
        return centerA < centerB ? -1 : 1;
    }

    i32 sortAlongY(const void* a,  const void* b)
    {
        auto nodeA = static_cast<const BVHNode*>(a);
        auto nodeB = static_cast<const BVHNode*>(b);
        f32 centerA = (nodeA->m_aabb.m_pMin.y + nodeA->m_aabb.m_pMax.y) * .5f;
        f32 centerB = (nodeB->m_aabb.m_pMin.y + nodeB->m_aabb.m_pMax.y) * .5f;
        return centerA < centerB ? -1 : 1;
    }

    i32 sortAlongZ(const void* a,  const void* b)
    {
        auto nodeA = static_cast<const BVHNode*>(a);
        auto nodeB = static_cast<const BVHNode*>(b);
        f32 centerA = (nodeA->m_aabb.m_pMin.z + nodeA->m_aabb.m_pMax.z) * .5f;
        f32 centerB = (nodeB->m_aabb.m_pMin.z + nodeB->m_aabb.m_pMax.z) * .5f;
        return centerA < centerB ? -1 : 1;
    }

    void MeshBVH::recursiveBuild(u32 start, u32 end, BVHNode* node)
    {
        // terminate when we arrive at a leaf node where there is only single triangle
        if (end - start <= 1)
        {
            node->m_mesh = m_owner;
            node->m_aabb = leafNodes[start].m_aabb;
            node->m_submeshIndex = leafNodes[start].m_submeshIndex;
            node->m_triangleIndex = leafNodes[start].m_triangleIndex;
            return;
        }
        u32 axis = rand() % 3u;
        if (axis == 0)
            std::qsort(leafNodes.data(), end - start, sizeof(BVHNode), sortAlongX);
        else if (axis == 1)
            std::qsort(leafNodes.data(), end - start, sizeof(BVHNode), sortAlongY);
        else if (axis == 2)
            std::qsort(leafNodes.data(), end - start, sizeof(BVHNode), sortAlongZ);

        u32 split = (start + end) / 2u;

        node->m_leftChild = allocNode(); 
        recursiveBuild(start, split, node->m_leftChild);
        node->bound(node->m_leftChild);

        node->m_rightChild = allocNode(); 
        recursiveBuild(split, end, node->m_rightChild);
        node->bound(node->m_rightChild);
    }

    MeshRayHit MeshBVH::trace(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        return root->trace(objectSpaceRo, objectSpaceRd);
    }

    BVHNode* buildMeshBVH(Mesh* mesh)
    {
        BVHNode* root = new BVHNode();
        u32 totalNumTriangles = 0u;
        for (u32 sm = 0; sm < mesh->m_subMeshes.size(); ++sm)
        {
            auto subMesh = mesh->m_subMeshes[sm];
            totalNumTriangles += subMesh->m_triangles.m_numVerts / 3;
        }

        // initialize leaf nodes
        std::vector<BVHNode*> leafNodes(totalNumTriangles);
        u32 triangleCounter = 0u;
        for (u32 sm = 0; sm < mesh->m_subMeshes.size(); ++sm)
        {
            auto subMesh = mesh->m_subMeshes[sm];
            u32 numTriangles = subMesh->m_triangles.m_numVerts / 3;
            for (u32 f = 0; f < numTriangles; ++f)
            {
                leafNodes[triangleCounter]= new BVHNode();
                leafNodes[triangleCounter]->bound(mesh->getTriangle(sm, f));
                leafNodes[triangleCounter]->m_submeshIndex = sm;
                leafNodes[triangleCounter++]->m_triangleIndex = f;
            }
        }

        // fix rand() seed
        srand(1);
        buildBVH(mesh, leafNodes, 0, (u32)leafNodes.size(), root);
        return root;
    }

    void buildBVH(Cyan::Mesh* mesh, std::vector<BVHNode*>& leafNodes, u32 start, u32 end, BVHNode* parent)
    {
        // terminate when we arive at a leaf node where there is only single triangle
        if (end - start <= 1)
        {
            parent->m_mesh = mesh;
            parent->m_aabb = leafNodes[start]->m_aabb;
            parent->m_submeshIndex = leafNodes[start]->m_submeshIndex;
            parent->m_triangleIndex = leafNodes[start]->m_triangleIndex;
            return;
        }

        // sort sub triangle groups and divide evenly
        auto sortAlongX = [](const void* a,  const void* b)
        {
            auto nodeA = static_cast<const BVHNode*>(a);
            auto nodeB = static_cast<const BVHNode*>(b);
            f32 centerA = (nodeA->m_aabb.m_pMin.x + nodeA->m_aabb.m_pMax.x) * .5f;
            f32 centerB = (nodeA->m_aabb.m_pMin.x + nodeA->m_aabb.m_pMax.x) * .5f;
            if (centerA < centerB)
                return -1;
            else
                return 1; 
        };

        auto sortAlongY = [](const void* a,  const void* b)
        {
            auto nodeA = static_cast<const BVHNode*>(a);
            auto nodeB = static_cast<const BVHNode*>(b);
            f32 centerA = (nodeA->m_aabb.m_pMin.x + nodeA->m_aabb.m_pMax.y) * .5f;
            f32 centerB = (nodeA->m_aabb.m_pMin.x + nodeA->m_aabb.m_pMax.y) * .5f;
            if (centerA < centerB)
                return -1;
            else
                return 1; 
        };

        auto sortAlongZ = [](const void* a,  const void* b)
        {
            auto nodeA = static_cast<const BVHNode*>(a);
            auto nodeB = static_cast<const BVHNode*>(b);
            f32 centerA = (nodeA->m_aabb.m_pMin.z + nodeA->m_aabb.m_pMax.z) * .5f;
            f32 centerB = (nodeA->m_aabb.m_pMin.z + nodeA->m_aabb.m_pMax.z) * .5f;
            if (centerA < centerB)
                return -1;
            else
                return 1; 
        };

        u32 axis = rand() % 3u;
        if (axis == 0)
            std::qsort(leafNodes.data(), end - start, sizeof(BVHNode*), sortAlongX);
        else if (axis == 1)
            std::qsort(leafNodes.data(), end - start, sizeof(BVHNode*), sortAlongY);
        else if (axis == 2)
            std::qsort(leafNodes.data(), end - start, sizeof(BVHNode*), sortAlongZ);

        u32 split = (start + end) / 2u;

        
        parent->m_leftChild = new BVHNode(); 
        buildBVH(mesh, leafNodes, start, split, parent->m_leftChild);
        parent->bound(parent->m_leftChild);

        parent->m_rightChild = new BVHNode(); 
        buildBVH(mesh, leafNodes, split, end, parent->m_rightChild);
        parent->bound(parent->m_rightChild);
    }

    MeshRayHit BVHNode::trace(glm::vec3& ro, glm::vec3& rd)
    {
        MeshRayHit meshRayHit;
        if (m_aabb.intersectRay(ro, rd) > 0.f)
        {
            // leaf node
            if (m_submeshIndex >= 0 && m_triangleIndex >= 0)
            {
                auto tri = m_mesh->getTriangle(m_submeshIndex, m_triangleIndex);
                f32 t = tri.intersectRay(ro, rd);
                if (t > 0.f)
                {
                    meshRayHit.mesh = m_mesh;
                    meshRayHit.t = t;
                    meshRayHit.smIndex = this->m_submeshIndex;
                    meshRayHit.triangleIndex = this->m_triangleIndex;
                }
                return meshRayHit;
            }

            // interior node
            auto hitLeft = m_leftChild->trace(ro, rd);
            auto hitRight = m_rightChild->trace(ro, rd);
            if (hitLeft.t < hitRight.t)
                meshRayHit = hitLeft;
            else
                meshRayHit = hitRight;
        }
        return meshRayHit;
    }

    bool BVHNode::traceVisibility(glm::vec3& ro, glm::vec3& rd)
    {
        if (m_aabb.intersectRay(ro, rd) > 0.f)
        {
            // leaf node
            if (m_submeshIndex >= 0 && m_triangleIndex >= 0)
            {
                auto tri = m_mesh->getTriangle(m_submeshIndex, m_triangleIndex);
                f32 t = tri.intersectRay(ro, rd);
                if (t > 0.f)
                    return true;
                return false;
            }
            return m_leftChild->traceVisibility(ro, rd) || m_rightChild->traceVisibility(ro, rd);
        }
        return false;
    }
}