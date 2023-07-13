#pragma once

#include <vector>

#include "MathLibrary.h"

namespace Cyan
{
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

    struct CORE_API Triangles : public Geometry
    {
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
        virtual Type getGeometryType() override { return Type::kTriangles; }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct PointCloud : public Geometry
    {
        struct Vertex
        {
            glm::vec3 pos;
        };

        virtual Type getGeometryType() override { return Type::kPointCloud; }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Quads : public Geometry
    {
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 texCoord;
        };

        virtual Type getGeometryType() override { return Type::kQuads; }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Lines : public Geometry
    {
        struct Vertex
        {
            glm::vec3 pos;
        };

        Lines() = default;
        Lines(const std::vector<Vertex>& vertices, const std::vector<u32>& indices);
        virtual Type getGeometryType() override { return Type::kLines; }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };
}
