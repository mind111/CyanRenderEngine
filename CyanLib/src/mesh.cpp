#include "Common.h"
#include "Mesh.h"
#include "Geometry.h"
#include "AssetManager.h"
#include "AssetImporter.h"

namespace Cyan
{
    StaticMesh::Submesh::Submesh(Geometry* inGeometry)
        : geometry(inGeometry), vb(nullptr), ib(nullptr), va(nullptr), index(-1)
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
                Triangles* triangles = static_cast<Triangles*>(geometry);
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
                auto triangles = static_cast<Triangles*>(geometry);

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
    }

    void StaticMesh::import()
    {
        AssetImporter::import(this);
    }

    void StaticMesh::onLoaded()
    {
        u32 count = numSubmeshes();
        for (i32 i = 0; i < count; ++i)
        {
            // for thread safety, not using [] operator here, because
            // another thread maybe calling addSubmesh() right now
            auto sm = getSubmesh(i);
            if (!sm->bInitialized)
            {
                sm->init();
            }
        }
        state = State::kInitialized;
    }

    void StaticMesh::addInstance(MeshInstance* inInstance) 
    { 
        std::lock_guard<std::mutex> lock(instanceMutex);
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

    void StaticMesh::addSubmeshDeferred(Geometry* inGeometry)
    {
        std::lock_guard<std::mutex> lock(submeshMutex);
        auto sm = std::make_shared<StaticMesh::Submesh>(inGeometry);
        submeshes.emplace_back(sm);
        onSubmeshAddedDeferred(sm.get());
    }

    void StaticMesh::onSubmeshAddedDeferred(Submesh* submesh)
    {
        std::lock_guard<std::mutex> lock(instanceMutex);
        for (auto inst : instances)
        {
            inst->addMaterial(AssetManager::getAsset<Material>("DefaultMaterial"));
        }

        AssetManager::onAssetPartiallyLoaded(this, [this, submesh](Asset* asset) {
            submesh->init();
        });
    }

    void StaticMesh::addSubmeshImmediate(Geometry* inGeometry)
    {
        std::lock_guard<std::mutex> lock(submeshMutex);
        auto sm = std::make_shared<StaticMesh::Submesh>(inGeometry);
        submeshes.emplace_back(sm);
        onSubmeshAddedImmediate(sm.get());
    }

    void StaticMesh::onSubmeshAddedImmediate(Submesh* submesh)
    {
        std::lock_guard<std::mutex> lock(instanceMutex);
        for (auto inst : instances)
        {
            inst->addMaterial(AssetManager::getAsset<Material>("DefaultMaterial"));
        }
        submesh->init();
    }

#if 0
    MeshRayHit Mesh::bruteForceIntersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        MeshRayHit globalHit;

        for (u32 i = 0; i < m_subMeshes.size(); ++i)
        {
            auto sm = m_subMeshes[i];
            u32 numTriangles = sm->m_triangles.m_numVerts / 3;
            for (u32 j = 0; j < numTriangles; ++j)
            {
                Triangle tri = {
                    sm->m_triangles.positions[j * 3],
                    sm->m_triangles.positions[j * 3 + 1],
                    sm->m_triangles.positions[j * 3 + 2]
                };

                float currentHit = tri.intersectRay(objectSpaceRo, objectSpaceRd); 
                if (currentHit > 0.f && currentHit < globalHit.t)
                {
                    globalHit.smIndex = i;
                    globalHit.triangleIndex = j;
                    globalHit.t = currentHit;
                    globalHit.mesh = this;
                }
            }
        }

        return globalHit;
    }

    bool Mesh::bruteForceVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        for (u32 i = 0; i < m_subMeshes.size(); ++i)
        {
            auto sm = m_subMeshes[i];
            u32 numTriangles = sm->m_triangles.m_numVerts / 3;
            for (u32 j = 0; j < numTriangles; ++j)
            {
                Triangle tri = {
                    sm->m_triangles.positions[j * 3],
                    sm->m_triangles.positions[j * 3 + 1],
                    sm->m_triangles.positions[j * 3 + 2]
                };

                float currentHit = tri.intersectRay(objectSpaceRo, objectSpaceRd); 
                if (currentHit > 0.f)
                    return true;
            }
        }
        return false;
    }

    bool Mesh::bvhVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        auto bvhRoot = m_bvh->root;
        return bvhRoot->traceVisibility(objectSpaceRo, objectSpaceRd);
    }

    MeshRayHit Mesh::bvhIntersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        return m_bvh->trace(objectSpaceRo, objectSpaceRd);
    }

    MeshRayHit Mesh::intersectRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        MeshRayHit meshRayHit;
        if (m_bvh)
            meshRayHit = bvhIntersectRay(objectSpaceRo, objectSpaceRd);
        else
            meshRayHit = bruteForceIntersectRay(objectSpaceRo, objectSpaceRd);
        if (meshRayHit.smIndex < 0 || meshRayHit.triangleIndex < 0)
            meshRayHit.t = -1.f;
        return meshRayHit; 
    }

    // coarse binary ray test
    bool Mesh::castVisibilityRay(glm::vec3& objectSpaceRo, glm::vec3& objectSpaceRd)
    {
        if (m_bvh)
            return bvhVisibilityRay(objectSpaceRo, objectSpaceRd);
        else
            return bruteForceVisibilityRay(objectSpaceRo, objectSpaceRd);
    }

    MeshInstance* Mesh::createInstance(Scene* scene)
    {
        MeshInstance* instance = new MeshInstance;
        instance->m_mesh = this;
        u32 numSubMeshes = (u32)this->m_subMeshes.size();
        instance->m_matls = (MaterialInstance**)CYAN_ALLOC(sizeof(MaterialInstance*) * numSubMeshes);
        instance->m_lightMap = nullptr;
#if 1
        // apply obj material defined in the asset
        if (!m_objMaterials.empty())
        {
            for (u32 sm = 0; sm < numSubMeshes; ++sm)
            {
                i32 matlIdx = m_subMeshes[sm]->m_materialIdx;
                if (matlIdx >= 0)
                {
                    auto& objMatl = m_objMaterials[matlIdx];
                    PbrMaterialParam params = { };
                    params.flatBaseColor = glm::vec4(objMatl.diffuse, 1.f);
                    // hard-coded default roughness/metallic for obj materials
                    params.kMetallic = 0.02f;
                    params.kRoughness = 0.5f;
                    params.hasBakedLighting = 0.f;
                    params.indirectDiffuseScale = 1.f;
                    params.indirectSpecularScale = 1.f;
                    auto matl = new StandardPbrMaterial(params);
                    instance->m_matls[sm] = matl->m_materialInstance;
                    scene->addStandardPbrMaterial(matl);
                }
            }
        }
#endif
        return instance;
    }
#endif
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