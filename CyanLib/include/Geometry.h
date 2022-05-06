#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "VertexArray.h"
#include "Shader.h"
#include "Material.h"

namespace Cyan
{
    enum class VertexAttribFlags
    {
        kHasPosition = (1 << 0),
        kHasNormal = (1 << 1),
        kHasTangent = (1 << 2),
        kHasTexCoord0 = (1 << 3),
        kHasTexCoord1 = (1 << 4)
    };

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
                return ((u8)VertexAttribFlags::kHasPosition
                    | (u8)VertexAttribFlags::kHasNormal
                    | (u8)VertexAttribFlags::kHasTangent
                    | (u8)VertexAttribFlags::kHasTexCoord0
                    | (u8)VertexAttribFlags::kHasTexCoord1);
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
                return ((u8)VertexAttribFlags::kHasPosition);
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
                return ((u8)VertexAttribFlags::kHasPosition
                    | (u8)VertexAttribFlags::kHasNormal
                    | (u8)VertexAttribFlags::kHasTexCoord0);
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
                return ((u8)VertexAttribFlags::kHasPosition);
            }
        };

        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };
}

#if 0
struct PointGroup
{
    PointGroup(u32 size);
    void push(glm::vec3& position);
    void draw(glm::mat4& mvp);
    void reset();
    void clear();
    void setColor(const glm::vec4& color);

    struct Point
    {
        glm::vec3 m_position;
    };

    std::vector<Point> m_points;
    u32 kMaxNumPoints;
    u32 m_numPoints;
    Shader* m_shader;
    Cyan::MaterialInstance* matl;
    VertexArray* m_vertexArray;
};

// todo: line segment rendered using a cylinder mesh
struct Line
{
    void init();
    Line& setVertices(glm::vec3 v0, glm::vec3 v1);
    Line& setColor(glm::vec4 color);
    void draw(glm::mat4& mvp);

    glm::vec3 m_vertices[2];
    Cyan::MaterialInstance* matl;
    GLuint vbo, vao;
};

struct Line2D
{
    Line2D(const glm::vec3& v0, const glm::vec3& v1) 
        : m_vertices{ v0, v1 }
    {
        init();
    }
    void init();
    void setVerts(glm::vec3& v0, glm::vec3& v1);
    void setColor(const glm::vec4& color);
    void draw();

    glm::vec3 m_vertices[2];
    glm::vec4 m_color;
    GLuint vbo;
    VertexArray* m_vertexArray;
    Cyan::MaterialInstance* matl;
};

struct Quad
{
    glm::vec2 m_pos; // [-1.f, 1.f]
    float w, h;      // [0.f, 2.f]
    glm::vec2 m_vertices[6];
    GLuint vao;
    GLuint vbo;
    Cyan::MaterialInstance* matl;

    Quad();
    ~Quad();
    void init(glm::vec2 pos, float width, float height);
    void draw();
};
#endif

struct Triangle
{
    glm::vec3 m_vertices[3];
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd);
};

// SoA data oriented triangle mesh meant for imrpoving ray tracing procedure
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

#if 0

    Cyan::Mesh* mesh = nullptr;
    Cyan::Mesh* meshInst = nullptr;
    Cyan::Material<Cyan::ConstantColorMatl>* matl = nullptr;
    // debug rendering
    glm::vec3 vertices[8];
    glm::vec3 color;
    struct VertexArray* vertexArray;
    static Shader* shader;
    Cyan::MaterialInstance* matl;
#endif

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

