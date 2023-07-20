#include "StaticMesh.h"
#include "GfxContext.h"

namespace Cyan
{
    StaticMesh::StaticMesh(const char* name, u32 numSubMeshes)
        : Asset(name), m_numSubMeshes(numSubMeshes)
    {
        m_subMeshes.resize(numSubMeshes);
    }

    StaticMesh::~StaticMesh()
    {

    }

    std::unique_ptr<StaticMeshInstance> StaticMesh::createInstance(const Transform& localToWorld)
    {
        return std::move(std::make_unique<StaticMeshInstance>(this, localToWorld));
    }

    void StaticMesh::removeInstance()
    {

    }

#define ENQUEUE_GFX_TASK(task)

    void StaticMesh::setSubMesh(std::unique_ptr<SubMesh> sm, u32 slot)
    {
        assert(slot < m_numSubMeshes);

        ENQUEUE_GFX_TASK([this, std::move(sm), slot]() {
            sm->createGfxProxy();
            m_subMeshes[slot] = std::move(sm);
        })
    }

    StaticMesh::SubMesh::SubMesh(StaticMesh* inParent, std::unique_ptr<Geometry> inGeometry)
        : parent(inParent), geometry(std::move(inGeometry))
    {
    }

    StaticMesh::SubMesh::~SubMesh()
    {

    }

    void StaticMesh::SubMesh::createGfxProxy()
    {
        // create gfx proxy here
        gfxProxy = GfxContext::get()->createGfxStaticSubMesh(geometry.get());
    }

    StaticMeshInstance::StaticMeshInstance(StaticMesh* parent, const Transform& localToWorld)
        : m_parent(parent), m_localToWorldTransform(localToWorld), m_localToWorldMatrix(localToWorld.toMatrix())
    {
        // materials.resize(parent->numSubMeshes());
    }

    StaticMeshInstance::~StaticMeshInstance()
    {

    }

    void StaticMeshInstance::setLocalToWorldTransform(const Transform& localToWorld)
    {
        m_localToWorldTransform = localToWorld;
        m_localToWorldMatrix = m_localToWorldTransform.toMatrix();
    }

}
