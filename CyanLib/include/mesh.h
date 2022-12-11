#pragma once
#include <string>
#include <map>
#include <array>
#include "glew.h"

#include "MathUtils.h"
#include "VertexBuffer.h"
#include "VertexArray.h"
#include "Material.h"
#include "Transform.h"
#include "Geometry.h"
#include "BVH.h"
#include "Asset.h"

struct Scene;

namespace Cyan
{
    // submesh interface
    struct ISubmesh
    {
        virtual u32 numVertices() = 0;
        virtual u32 numIndices() = 0;
        virtual glm::vec3 getMin() = 0;
        virtual glm::vec3 getMax() = 0;
        virtual VertexArray* getVertexArray() = 0;
    };

    struct Mesh : public Asset
    {
        template <typename Geometry>
        struct Submesh : public ISubmesh
        {
            Submesh(const std::vector<typename Geometry::Vertex>& vertices, const std::vector<u32>& indices)
            {
                geometry.vertices = std::move(vertices);
                geometry.indices = std::move(indices);

                init();
            }

            Submesh(const Geometry& srcGeom)
            {
                // todo: transfer geometry data

                init();
            }

            virtual u32 numVertices() override { return geometry.numVertices(); }
            virtual u32 numIndices() override { return geometry.numIndices(); }
            virtual VertexArray* getVertexArray() { return va; }
            const std::vector<typename Geometry::Vertex>& getVertices() { return geometry.vertices; }
            const std::vector<u32>& getIndices() { return geometry.indices; }
            void setGeometryData(std::vector<typename Geometry::Vertex>& vertices, const std::vector<u32>& indices)
            {
                geometry.vertices = vertices;
                geometry.indices = indices;

                // release old resources if it exists
                if (va)
                {
                    va->release();
                    delete va;
                }

                // upload gpu data
                init();
            }

            virtual glm::vec3 getMin() override { return pmin; }
            virtual glm::vec3 getMax() override { return pmax; }

        private:
            /*
            * init vertex buffer and vertex array
            */
            void init()
            {
                VertexSpec vertexSpec;

                u8 flags = Geometry::Vertex::getFlags();
                if (flags & VertexAttribFlag_kPosition)
                {
                    vertexSpec.addAttribute({ "POSITION", 3, 0 });
                }
                if (flags & VertexAttribFlag_kNormal)
                {
                    vertexSpec.addAttribute({ "NORMAL", 3, 0 });
                }
                if (flags & VertexAttribFlag_kTangent)
                {
                    vertexSpec.addAttribute({ "TANGENT", 4, 0 });
                }
                if (flags & VertexAttribFlag_kTexCoord0)
                {
                    vertexSpec.addAttribute({ "TEXCOORD0", 2, 0 });
                }
                if (flags & VertexAttribFlag_kTexCoord1)
                {
                    vertexSpec.addAttribute({ "TEXCOORD1", 2, 0 });
                }
                auto vb = createVertexBuffer(geometry.vertices.data(), sizeof(Geometry::Vertex) * geometry.vertices.size(), std::move(vertexSpec));
                va = createVertexArray(vb, &geometry.indices);

                for (u32 i = 0; i < numVertices(); ++i)
                {
                    pmin.x = Min(pmin.x, geometry.vertices[i].pos.x);
                    pmin.y = Min(pmin.y, geometry.vertices[i].pos.y);
                    pmin.z = Min(pmin.z, geometry.vertices[i].pos.z);

                    pmax.x = Max(pmax.x, geometry.vertices[i].pos.x);
                    pmax.y = Max(pmax.y, geometry.vertices[i].pos.y);
                    pmax.z = Max(pmax.z, geometry.vertices[i].pos.z);
                }
            }

            Geometry geometry;
            VertexArray* va;
            glm::vec3 pmin = glm::vec3(FLT_MAX);
            glm::vec3 pmax = glm::vec3(-FLT_MAX);
        };

        Mesh() = default;
        Mesh(const char* meshName, const std::vector<ISubmesh*>& srcSubmeshes)
            : name(meshName) 
        { 
            submeshes = std::move(srcSubmeshes);
        }
        
        // Asset interface
        virtual std::string getAssetObjectTypeDesc() override { return typeDesc; }
        static std::string getAssetClassTypeDesc() { return typeDesc; }

        ISubmesh* getSubmesh(u32 index) { return submeshes[index]; }
        const BoundingBox3D& getAABB() { return aabb; }
        u32 numSubmeshes() { return submeshes.size(); }
        void init();

        
        static std::string typeDesc;
        std::string name;
        BoundingBox3D aabb;
        std::vector<ISubmesh*> submeshes;
    };

    struct MeshInstance {
        MeshInstance(Mesh* base)
            : parent(base) {
            materials.resize(base->numSubmeshes());
        }

        Material* getMaterial(u32 index) {
            return materials[index];
        }

        void setMaterial(Material* matl) {
            for (u32 i = 0; i < parent->numSubmeshes(); ++i) {
                setMaterial(matl, i);
            }
        }

        void setMaterial(Material* matl, u32 index) {
            materials[index] = matl;
        }

        BoundingBox3D getAABB(const glm::mat4& worldTransform) {
            BoundingBox3D parentAABB = parent->getAABB();
            BoundingBox3D aabb;
            aabb.bound(worldTransform * parentAABB.pmin);
            aabb.bound(worldTransform * parentAABB.pmax);
            return aabb;
        }

        Mesh* parent = nullptr;
        std::vector<Material*> materials;
    };
}

extern float cubeVertices[108];
