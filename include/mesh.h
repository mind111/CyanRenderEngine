#pragma once
#include <string>
#include <map>
#include "glew.h"

#include "MathUtils.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "Material.h"
#include "Transform.h"

namespace Cyan
{
    struct MeshInstance;
    struct SkeletalMeshInstance;

    struct Mesh
    {
        MeshInstance* createInstance();

        struct SubMesh
        {
            std::string m_name;
            VertexArray* m_vertexArray;
            u32 m_numVerts;
        };

        std::string m_name;
        glm::mat4 m_normalization;
        std::vector<SubMesh*> m_subMeshes;
    };

    struct MeshInstance
    {
        void setMaterial(u32 index, MaterialInstance* matl);

        Mesh* m_mesh;
        MaterialInstance** m_matls;
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

extern float cubeVertices[108];