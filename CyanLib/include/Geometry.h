#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "VertexArray.h"
#include "Shader.h"
#include "Material.h"

namespace Cyan
{
    /* Note:
    * maybe it's even better to just pull Vertex struct out and make it template to allow it to be more flexible ...?
    */
    struct Geometry
    {
        enum class Type
        {
            kTriangles = 0,
            kQuads,
            kPointCloud,
            kLines,
        };

        virtual Type getGeometryType() = 0;
        virtual u32 numVertices() = 0;
        virtual u32 numIndices() = 0;
    };

    struct Triangles : public Geometry
    {
        virtual Type getGeometryType() override { return Type::kTriangles; }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec4 tangent;
            glm::vec2 texCoord0;
            glm::vec2 texCoord1;
        };

        Triangles() = default;
        Triangles(const std::vector<Triangles::Vertex>& inVertices, const std::vector<u32>& inIndices);

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct PointCloud : public Geometry
    {
        virtual Type getGeometryType() override { return Type::kPointCloud; }
        u32 numVertices() { return vertices.size(); }
        u32 numIndices() { return indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;
        };

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Quads : public Geometry
    {
        virtual Type getGeometryType() override { return Type::kQuads; }
        u32 numVertices() { return vertices.size(); }
        u32 numIndices() { return indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 texCoord;
        };

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Lines : public Geometry
    {
        virtual Type getGeometryType() override { return Type::kLines; }
        u32 numVertices() { return vertices.size(); }
        u32 numIndices() { return indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;
        };

        Lines() = default;
        Lines(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };
}

struct Triangle
{
    glm::vec3 m_vertices[3];
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd);
};

struct BoundingBox3D
{
    // in object space
    glm::vec4 pmin;
    glm::vec4 pmax;

    BoundingBox3D();

    void bound(const BoundingBox3D& aabb);
    void bound(const glm::vec3& v3);
    void bound(const glm::vec4& v4);
    void bound(const Triangle& tri);

    /*
    * Reset bounds and vertex data
    */
    void reset();

    /*
    * Recompute and udpate bounds and vertex data
    */
    // void upload();
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd);
};

