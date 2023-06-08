#include "Common.h"
#include "Mesh.h"
#include "Geometry.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include "GraphicsSystem.h"
#include "World.h"
#include "StaticMeshComponent.h"

namespace Cyan
{
    StaticMesh::Submesh::Submesh(StaticMesh* inOwner, i32 submeshIndex, std::unique_ptr<Geometry> inGeometry)
        : owner(inOwner), index(submeshIndex), geometry(std::move(inGeometry)), bInited(false), vb(nullptr), ib(nullptr), va(nullptr)
    {
        assert(index < owner->getNumSubmeshes());
    }

    void StaticMesh::Submesh::init()
    {
        assert(!bInited);
        if (!bInited)
        {
            switch (geometry->getGeometryType())
            {
            case Geometry::Type::kTriangles: {
                auto triangles = static_cast<Triangles*>(geometry.get());

                u32 sizeInBytes = sizeof(triangles->vertices[0]) * triangles->vertices.size();
                assert(sizeInBytes > 0);

                VertexBuffer::Spec spec;
                spec.addVertexAttribute("Position", VertexBuffer::Attribute::Type::kVec3);
                spec.addVertexAttribute("Normal", VertexBuffer::Attribute::Type::kVec3);
                spec.addVertexAttribute("Tangent", VertexBuffer::Attribute::Type::kVec4);
                spec.addVertexAttribute("TexCoord0", VertexBuffer::Attribute::Type::kVec2);
                spec.addVertexAttribute("TexCoord1", VertexBuffer::Attribute::Type::kVec2);
                vb = std::make_shared<VertexBuffer>(spec, triangles->vertices.data(), sizeInBytes);
                ib = std::make_shared<IndexBuffer>(triangles->indices);
                va = std::make_shared<VertexArray>(vb.get(), ib.get());
                va->init();
            } break;
            case Geometry::Type::kLines:
                // todo: implement this  
            case Geometry::Type::kPointCloud:
                // todo: implement this  
            case Geometry::Type::kQuads:
                // todo: implement this  
            default:
                assert(0);
            }

            bInited = true;

            onInited();
        }
    }

    void StaticMesh::Submesh::onInited()
    {
        owner->onSubmeshInited(index);
    }

    StaticMesh::Instance::Instance(StaticMeshComponent* owner, std::shared_ptr<StaticMesh> inParent, const Transform& inLocalToWorld)
        : meshComponent(owner), parent(inParent), localToWorld(inLocalToWorld)
    {
        assert(parent != nullptr);
        m_materials.resize(parent->getNumSubmeshes());
        auto defaultMaterial = AssetManager::findAsset<Material>("M_DefaultOpaque");
        assert(defaultMaterial != nullptr);
        for (i32 i = 0; i < parent->getNumSubmeshes(); ++i)
        {
            m_materials[i] = defaultMaterial;
        }
        parent->addInstance(this);
    }

    std::shared_ptr<Material>& StaticMesh::Instance::operator[](i32 index)
    {
        assert(index < parent->getNumSubmeshes());
        return m_materials[index];
    }

    void StaticMesh::Instance::onMeshInited()
    {
        assert(meshComponent != nullptr);
        Scene* scene = meshComponent->getWorld()->getScene();
        scene->addStaticMeshInstance(this);
    }

    void StaticMesh::Instance::addToScene()
    {
        assert(meshComponent != nullptr);
        Scene* scene = meshComponent->getWorld()->getScene();
        scene->addStaticMeshInstance(this);
    }

    StaticMesh::StaticMesh(const char* name, i32 numSubmeshes)
        : Asset(name), m_numSubmeshes(numSubmeshes)
    {
        m_submeshes.resize(m_numSubmeshes);
        for (i32 i = 0; i < m_numSubmeshes; ++i)
        {
            m_submeshes[i] = nullptr;
        }
    }

    StaticMesh::Submesh* StaticMesh::createSubmesh(i32 index, std::unique_ptr<Geometry> geometry)
    {
        assert(index < m_numSubmeshes);
        assert(m_submeshes[index] == nullptr);
        m_submeshes[index] = std::make_shared<Submesh>(this, index, std::move(geometry));
        onSubmeshLoaded(index);
        return m_submeshes[index].get();
    }

    i32 StaticMesh::getNumSubmeshes() 
    { 
        assert(m_numSubmeshes == m_submeshes.size());
        return m_numSubmeshes; 
    }

    std::shared_ptr<StaticMesh::Submesh>& StaticMesh::getSubmesh(i32 smIndex)
    {
        assert(smIndex < m_numSubmeshes);
        return m_submeshes[smIndex];
    }

    std::shared_ptr<StaticMesh::Submesh>& StaticMesh::operator[](i32 index)
    {
        return getSubmesh(index);
    }

    void StaticMesh::addInstance(Instance* instance)
    {
        m_instances.push_back(instance);
    }

    void StaticMesh::onSubmeshLoaded(i32 index)
    {
        assert(index < m_numSubmeshes);
        assert(m_numLoadedSubmeshes < m_numSubmeshes);
        m_numLoadedSubmeshes++;
        ENQUEUE_GFX_COMMAND(InitSubmeshCommand, [this, index]() {
            m_submeshes[index]->init();
        });

    }

    void StaticMesh::onSubmeshInited(i32 index)
    {
        assert(index < m_numSubmeshes);
        assert(m_numInitedSubmeshes < m_numSubmeshes);
        m_numInitedSubmeshes++;
        if (m_numInitedSubmeshes == m_numSubmeshes)
        {
            for (auto instance : m_instances)
            {
                instance->addToScene();
            }
        }
    }
}

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