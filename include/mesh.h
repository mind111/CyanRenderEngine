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

namespace Cyan
{
    struct MeshInstance;
    struct SkeletalMeshInstance;

    // TODO: how to distinguish between 2D mesh and 3D mesh
    struct Mesh
    {
        void onFinishLoading();
        MeshInstance* createInstance();

        struct SubMesh
        {
            VertexArray* m_vertexArray;
            u32 m_numVerts;
            std::vector<Triangle> m_triangles;
        };

        std::string m_name;
        bool m_shouldNormalize;
        glm::mat4 m_normalization;
        std::vector<SubMesh*> m_subMeshes;
        BoundingBox3f m_aabb;
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
        BoundingBox3f& getAABB();
    };

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

    struct Joint {
        Joint* m_parent;
        Transform m_localTransform;
        glm::mat4 m_worldMatrix;
    };

    struct Skeleton {
        Joint* m_root;
        std::vector<Joint> m_joints;
    };

    struct SkeletalMesh : public Mesh {
        struct SubMesh {
            std::vector<glm::vec4> m_joints;
            std::vector<glm::vec4> m_weights;
        };
        Skeleton* m_skeleton;
        std::vector<SubMesh*> m_subMeshes;
    };

    struct SkeletalMeshInstance {
        SkeletalMesh* m_mesh;
        MaterialInstance** m_matls;
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