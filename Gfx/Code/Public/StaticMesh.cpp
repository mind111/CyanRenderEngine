#include "StaticMesh.h"

namespace Cyan
{
    StaticMesh::StaticMesh(const char* name, u32 numSubMeshes)
        : Asset(name), m_numSubMeshes(numSubMeshes)
    {
        m_subMeshes.resize(numSubMeshes);
    }

    StaticMeshInstance* StaticMesh::createInstance()
    {
    }

#define ENQUEUE_GFX_TASK(task)

    void StaticMesh::setSubMesh(std::unique_ptr<SubMesh> sm, u32 slot)
    {
        assert(slot < m_numSubMeshes);

        ENQUEUE_GFX_TASK([this, std::move(sm), slot]() {
            sm->initGfxData();
            m_subMeshes[slot] = std::move(sm);
        })
    }

    StaticMesh::SubMesh::SubMesh(StaticMesh* inParent, std::unique_ptr<Geometry> inGeometry)
        : parent(inParent), geometry(std::move(inGeometry))
    {
    }

    void StaticMesh::SubMesh::initGfxData()
    {
        if (bReadyForRendering != true)
        {
            bReadyForRendering = true;
        }
    }

    StaticMeshInstance::StaticMeshInstance(StaticMesh* inParent, const glm::mat4& inTransform)
        : parent(inParent), transform(inTransform)
    {
        materials.resize(parent->numSubMeshes());
    }
}
