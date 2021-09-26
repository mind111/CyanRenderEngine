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

    MeshInstance* Mesh::createInstance()
    {
        MeshInstance* instance = (MeshInstance*)CYAN_ALLOC(sizeof(MeshInstance));
        instance->m_mesh = this;
        u32 numSubMeshes = (u32)this->m_subMeshes.size();
        instance->m_matls = (MaterialInstance**)CYAN_ALLOC(sizeof(MaterialInstance*) * numSubMeshes);
        return instance;
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