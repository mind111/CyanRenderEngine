#pragma once

#include <vector>

#include "Geometry.h"

namespace Cyan 
{
#if 0
    struct BVHNode
    {
        struct Mesh*    m_mesh;
        BVHNode*        m_leftChild;
        BVHNode*        m_rightChild;
        BoundingBox3D   m_aabb;
        i32             m_submeshIndex;
        i32             m_triangleIndex;

        BVHNode()
            : m_mesh(nullptr), m_leftChild(nullptr), m_rightChild(nullptr), m_aabb{},
            m_submeshIndex(-1), m_triangleIndex(-1)
        { }

        void bound(BVHNode* node) 
        { 
            m_aabb.bound(node->m_aabb);
        }

        void bound(const Triangle& tri) 
        { 
            m_aabb.bound(tri);
        }

        struct MeshRayHit trace(glm::vec3& ro, glm::vec3& rd);
        bool traceVisibility(glm::vec3& ro, glm::vec3& rd);
    };

    struct MeshBVH
    {
        MeshBVH(struct Mesh* mesh);

        struct Mesh* m_owner;
        BVHNode* root;
        std::vector<BVHNode> leafNodes;
        std::vector<BVHNode> nodes;
        u32      numNodes;
        BVHNode* allocNode();
        void build();
        void onBuildFinished();
        void recursiveBuild(u32 start, u32 end, BVHNode* node);
        MeshRayHit trace(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd);
    };

    BVHNode* buildMeshBVH(struct Mesh* mesh);
    void buildBVH(struct Mesh* mesh, std::vector<BVHNode*>& leafNodes, u32 start, u32 end, BVHNode* parent);
#endif
}