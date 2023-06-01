#pragma once
#include <string>
#include <map>
#include <array>
#include <glew/glew.h>
#include <mutex>

#include "Asset.h"
#include "MathUtils.h"
#include "Material.h"
#include "Geometry.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "Transform.h"

namespace Cyan
{
    struct Geometry;
    class StaticMeshComponent;
    class Scene;

    class StaticMesh : public Asset
    {
    public:

        struct Submesh
        {
            Submesh(StaticMesh* inOwner, i32 submeshIndex, std::unique_ptr<Geometry> inGeometry);
            ~Submesh() { }

            void init();
            void onInited();
            i32 numVertices() { return geometry->numVertices(); }
            i32 numIndices() { return geometry->numIndices(); }

            StaticMesh* owner = nullptr;
            i32 index = -1; // index into parent's submesh array
            std::unique_ptr<Geometry> geometry = nullptr;
            bool bInited = false;
            std::shared_ptr<VertexBuffer> vb = nullptr;
            std::shared_ptr<IndexBuffer> ib = nullptr;
            std::shared_ptr<VertexArray> va = nullptr;
        };

        struct Instance
        {
            Instance(StaticMeshComponent* owner, std::shared_ptr<StaticMesh> inParent, const Transform& inLocalToWorld);
            ~Instance() { }

            void onMeshInited();
            void addToScene();
            std::shared_ptr<Material>& operator[](i32 index);

            StaticMeshComponent* meshComponent = nullptr;
            std::shared_ptr<StaticMesh> parent = nullptr;
            Transform localToWorld;
            std::vector<std::shared_ptr<Material>> m_materials;
        };

        StaticMesh(const char* m_name, i32 numSubmeshes);
        ~StaticMesh() { }

        static const char* getAssetTypeName() { return "StaticMesh"; }

        Submesh* createSubmesh(i32 index, std::unique_ptr<Geometry> geometry);
        std::shared_ptr<Submesh>& operator[](i32 index);
        i32 getNumSubmeshes();
        void onSubmeshLoaded(i32 index);
        void onSubmeshInited(i32 index);
        void addInstance(Instance* instance);

    private:
        std::shared_ptr<Submesh>& getSubmesh(i32 smIndex);

        i32 m_numSubmeshes = 0;
        i32 m_numLoadedSubmeshes = 0;
        i32 m_numInitedSubmeshes = 0;
        std::vector<std::shared_ptr<Submesh>> m_submeshes;
        std::vector<Instance*> m_instances;
    };
}

extern float cubeVertices[108];
