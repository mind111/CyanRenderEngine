#include "Common.h"
#include "Mesh.h"
#include "Geometry.h"
#include "AssetManager.h"
#include "AssetImporter.h"

namespace Cyan
{
    std::vector<StaticMesh::Submesh::Desc> StaticMesh::g_submeshes;
    std::vector<StaticMesh::Vertex> StaticMesh::g_vertices;
    std::vector<u32> StaticMesh::g_indices;

    StaticMesh::Submesh::Submesh(StaticMesh* inOwner)
        : owner(inOwner), geometry(nullptr), vb(nullptr), ib(nullptr), va(nullptr), index(-1)
    {
    }

    StaticMesh::Submesh::Submesh(StaticMesh* inOwner, std::shared_ptr<Geometry> inGeometry)
        : owner(inOwner), geometry(inGeometry), vb(nullptr), ib(nullptr), va(nullptr), index(-1)
    {
    }

    StaticMesh::Submesh::Desc StaticMesh::getSubmeshDesc(StaticMesh::Submesh* submesh)
    {
        return g_submeshes[submesh->index];
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

            Desc desc = { };
            Geometry::Type type = geometry->getGeometryType();
            switch (type)
            {
            case Geometry::Type::kTriangles: {
                desc.vertexOffset = g_vertices.size();
                desc.numVertices = geometry->numVertices();
                desc.indexOffset = g_indices.size();
                desc.numIndices = geometry->numIndices();

                // pack the src geometry data into the global vertex/index buffer for static geometries
                Triangles* triangles = static_cast<Triangles*>(geometry.get());
                u32 start = g_vertices.size();
                g_vertices.resize(g_vertices.size() + geometry->numVertices());
                const auto& vertices = triangles->vertices;
                const auto& indices = triangles->indices;
                for (u32 v = 0; v < triangles->vertices.size(); ++v)
                {
                    g_vertices[start + v].pos = glm::vec4(vertices[v].pos, 1.f);
                    g_vertices[start + v].normal = glm::vec4(vertices[v].normal, 0.f);
                    g_vertices[start + v].tangent = vertices[v].tangent;
                    g_vertices[start + v].texCoord = glm::vec4(vertices[v].texCoord0, vertices[v].texCoord1);
                }
                g_indices.insert(g_indices.end(), indices.begin(), indices.end());
            } break;
            case Geometry::Type::kPointCloud:
            case Geometry::Type::kLines:
            default:
                assert(0);
            }

            index = g_submeshes.size();
            g_submeshes.push_back(desc);

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

            // upload data to these three global buffers as they just become out-dated on gpu end
            getGlobalVertexBuffer()->write(g_vertices, 0, sizeOfVector(g_vertices));
            getGlobalIndexBuffer()->write(g_indices, 0, sizeOfVector(g_indices));
            getGlobalSubmeshBuffer()->write(g_submeshes, 0, sizeOfVector(g_submeshes));
        }
        else
        {

        }
    }

    ShaderStorageBuffer* StaticMesh::getGlobalSubmeshBuffer()
    {
        static std::unique_ptr<ShaderStorageBuffer> g_submeshBuffer = std::make_unique<ShaderStorageBuffer>("SubmeshBuffer", sizeOfVector(g_submeshes));
        return g_submeshBuffer.get();
    }

    ShaderStorageBuffer* StaticMesh::getGlobalVertexBuffer()
    {
        static std::unique_ptr<ShaderStorageBuffer> g_vertexBuffer = std::make_unique<ShaderStorageBuffer>("VertexBuffer", sizeOfVector(g_vertices));
        return g_vertexBuffer.get();
    }

    ShaderStorageBuffer* StaticMesh::getGlobalIndexBuffer()
    {
        static std::unique_ptr<ShaderStorageBuffer> g_indexBuffer = std::make_unique<ShaderStorageBuffer>("IndexBuffer", sizeOfVector(g_indices));
        return g_indexBuffer.get();
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