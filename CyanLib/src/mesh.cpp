#include "Common.h"
#include "Mesh.h"
#include "Geometry.h"
#include "AssetManager.h"
#include "AssetImporter.h"

namespace Cyan
{
    StaticMesh::Submesh::Submesh(StaticMesh* inOwner)
        : owner(inOwner), geometry(nullptr), vb(nullptr), ib(nullptr), va(nullptr), index(-1)
    {
    }

    StaticMesh::Submesh::Submesh(StaticMesh* inOwner, std::shared_ptr<Geometry> inGeometry)
        : owner(inOwner), geometry(inGeometry), vb(nullptr), ib(nullptr), va(nullptr), index(-1)
    {
    }

    StaticMesh::SubmeshBuffer& StaticMesh::getSubmeshBuffer()
    {
        static SubmeshBuffer s_submeshBuffer("SubmeshBuffer");
        return s_submeshBuffer;
    }

    StaticMesh::GlobalVertexBuffer& StaticMesh::getGlobalVertexBuffer()
    {
        static GlobalVertexBuffer s_vertexBuffer("VertexBuffer");
        return s_vertexBuffer;
    }

    StaticMesh::GlobalIndexBuffer& StaticMesh::getGlobalIndexBuffer()
    {
        static GlobalIndexBuffer s_indexBuffer("IndexBuffer");
        return s_indexBuffer;
    }

    StaticMesh::Submesh::Desc StaticMesh::getSubmeshDesc(StaticMesh::Submesh* submesh)
    {
        return getSubmeshBuffer()[submesh->index];
    }

    void StaticMesh::Submesh::setGeometry(std::shared_ptr<Geometry> inGeometry)
    {
        assert(geometry == nullptr);
        geometry = inGeometry;

        AssetManager::deferredInitAsset(owner, [this](Asset* asset) {
            init();
        });
    }

    /**
     *  this function actually initialize rendering related data on Gpu, and it
     * makes Gfx API calls, thus requiring a valid GL context, thus need to be executed
     * on the main thread.
     */
    void StaticMesh::Submesh::init()
    {
        if (!bInitialized)
        {
            assert(geometry);

            // pack geometry data into global vertex / index buffer
            auto& gVertexBuffer = getGlobalVertexBuffer();
            auto& gIndexBuffer = getGlobalIndexBuffer();

            Desc desc = { };
            Geometry::Type type = geometry->getGeometryType();
            switch (type)
            {
            case Geometry::Type::kTriangles: {
                desc.vertexOffset = gVertexBuffer.getNumElements();
                desc.numVertices = geometry->numVertices();
                desc.indexOffset = gIndexBuffer.getNumElements();
                desc.numIndices = geometry->numIndices();

                // pack the src geometry data into the global vertex/index buffer for static geometries
                Triangles* triangles = static_cast<Triangles*>(geometry.get());
                u32 start = gVertexBuffer.getNumElements();
                gVertexBuffer.data.array.resize(gVertexBuffer.getNumElements() + geometry->numVertices());
                const auto& vertices = triangles->vertices;
                const auto& indices = triangles->indices;
                for (u32 v = 0; v < triangles->vertices.size(); ++v)
                {
                    gVertexBuffer[start + v].pos = glm::vec4(vertices[v].pos, 1.f);
                    gVertexBuffer[start + v].normal = glm::vec4(vertices[v].normal, 0.f);
                    gVertexBuffer[start + v].tangent = vertices[v].tangent;
                    gVertexBuffer[start + v].texCoord = glm::vec4(vertices[v].texCoord0, vertices[v].texCoord1);
                }
                gIndexBuffer.data.array.insert(gIndexBuffer.data.array.end(), indices.begin(), indices.end());
            } break;
            case Geometry::Type::kPointCloud:
            case Geometry::Type::kLines:
            default:
                assert(0);
            }

            auto& submeshBuffer = getSubmeshBuffer();
            index = submeshBuffer.getNumElements();
            submeshBuffer.addElement(desc);

            // todo: this makes the geometry data duplicated on the Gpu end, need to make it choosable whether a submesh should be both packed and initialized seperately
            // initialize vertex buffer object, index buffer object, and vertex array object
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

            bInitialized = true;
        }
        else
        {

        }
    }

    void StaticMesh::import()
    {
        if (state != State::kLoaded && state != State::kLoading)
        {
            state = State::kLoading;
            AssetImporter::import(this);
        }
    }

    void StaticMesh::onLoaded()
    {
        state = State::kLoaded;
    }

    void StaticMesh::addInstance(MeshInstance* inInstance)
    {
        instances.push_back(inInstance);
    }

    StaticMesh::Submesh* StaticMesh::getSubmesh(u32 index)
    {
        std::lock_guard<std::mutex> lock(submeshMutex);
        return submeshes[index].get();
    }

    u32 StaticMesh::numSubmeshes()
    {
        std::lock_guard<std::mutex> lock(submeshMutex);
        return submeshes.size();
    }

    u32 StaticMesh::numInstances()
    {
        return instances.size();
    }

    u32 StaticMesh::numVertices()
    {
        u32 totalNumVertices = 0u;
        u32 numSubmesh = numSubmeshes();
        for (u32 i = 0; i < numSubmesh; ++i)
        {
            auto sm = getSubmesh(i);
            totalNumVertices += sm->numVertices();
        }
        return totalNumVertices;
    }

    u32 StaticMesh::numIndices()
    {
        u32 totalNumIndices = 0u;
        u32 numSubmesh = numSubmeshes();
        for (u32 i = 0; i < numSubmesh; ++i)
        {
            auto sm = getSubmesh(i);
            totalNumIndices += sm->numIndices();
        }
        return totalNumIndices;
    }

    void StaticMesh::addSubmeshDeferred(Geometry* inGeometry)
    {
        std::lock_guard<std::mutex> lock(submeshMutex);
        auto sm = std::make_shared<StaticMesh::Submesh>(this, std::shared_ptr<Geometry>(inGeometry));
        submeshes.emplace_back(sm);
        onSubmeshAddedDeferred(sm.get());
    }

    void StaticMesh::onSubmeshAddedDeferred(Submesh* submesh)
    {
        std::lock_guard<std::mutex> lock(instanceMutex);
        for (auto inst : instances)
        {
            inst->onSubmeshAdded();
        }

        AssetManager::deferredInitAsset(this, [this, submesh](Asset* asset) {
            submesh->init();
            });
    }

    void StaticMesh::addSubmeshImmediate(Geometry* inGeometry)
    {
        std::lock_guard<std::mutex> lock(submeshMutex);
        auto sm = std::make_shared<StaticMesh::Submesh>(this, std::shared_ptr<Geometry>(inGeometry));
        submeshes.emplace_back(sm);
        onSubmeshAddedImmediate(sm.get());
    }

    void StaticMesh::onSubmeshAddedImmediate(Submesh* submesh)
    {
        std::lock_guard<std::mutex> lock(instanceMutex);
        for (auto inst : instances)
        {
            inst->onSubmeshAdded();
        }
        submesh->init();
    }

    void MeshInstance::onSubmeshAdded()
    {
        addMaterial(AssetManager::getAsset<Material>("DefaultMaterial"));
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