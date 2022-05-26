#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "VertexArray.h"
#include "Shader.h"
#include "Material.h"

namespace Cyan
{
#define VertexAttribFlag_kPosition (u8)VertexAttribute::Type::kPosition << 1
#define VertexAttribFlag_kNormal (u8)VertexAttribute::Type::kNormal << 1
#define VertexAttribFlag_kTangent (u8)VertexAttribute::Type::kTangent << 1
#define VertexAttribFlag_kTexCoord0 (u8)VertexAttribute::Type::kTexCoord0 << 1
#define VertexAttribFlag_kTexCoord1 (u8)VertexAttribute::Type::kTexCoord1 << 1

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
    };

    struct Triangles : public Geometry
    {
        static Type getTypeEnum() { return Type::kTriangles; }
        u32 numVertices() { return (u32)vertices.size(); }
        u32 numIndices() { return (u32)indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec4 tangent;
            glm::vec2 texCoord0;
            glm::vec2 texCoord1;

            static u8 getFlags()
            {
                return (VertexAttribFlag_kPosition
                    | VertexAttribFlag_kNormal
                    | VertexAttribFlag_kTangent 
                    | VertexAttribFlag_kTexCoord0 
                    | VertexAttribFlag_kTexCoord1);
            }
        };

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct PointCloud : public Geometry
    {
        static Type getTypeEnum() { return Type::kPointCloud; }
        u32 numVertices() { return vertices.size(); }
        u32 numIndices() { return indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;

            static u8 getFlags()
            {
                return VertexAttribFlag_kPosition;
            }
        };

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Quads : public Geometry
    {
        static Type getTypeEnum() { return Type::kQuads; }
        u32 numVertices() { return vertices.size(); }
        u32 numIndices() { return indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 texCoord;

            static u8 getFlags()
            {
                return (VertexAttribFlag_kPosition
                    | VertexAttribFlag_kNormal
                    | VertexAttribFlag_kTexCoord0);
            }
        };

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    struct Lines : public Geometry
    {
        static Type getTypeEnum() { return Type::kLines; }
        u32 numVertices() { return vertices.size(); }
        u32 numIndices() { return indices.size(); }

        struct Vertex
        {
            glm::vec3 pos;

            static u8 getFlags()
            {
                return VertexAttribFlag_kPosition;
            }
        };

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };
}

struct Triangle
{
    glm::vec3 m_vertices[3];
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd);
};

// SoA data oriented triangle mesh meant for improving ray tracing procedure
struct TriangleArray 
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> texCoords;
    u32 m_numVerts;
};

struct BoundingBox3D
{
    // in object space
    glm::vec4 pmin;
    glm::vec4 pmax;

    BoundingBox3D();

    void init();

    // void draw(glm::mat4& mvp);
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
    // void update();
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd);
};

