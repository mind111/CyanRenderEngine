#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "VertexArray.h"
#include "Shader.h"
#include "Material.h"

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
    GLuint m_vbo, m_vao;
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
    GLuint m_vbo;
    VertexArray* m_vertexArray;
    Cyan::MaterialInstance* matl;
};

struct Quad
{
    glm::vec2 m_pos; // [-1.f, 1.f]
    float w, h;      // [0.f, 2.f]
    glm::vec2 m_vertices[6];
    GLuint m_vao;
    GLuint m_vbo;
    Cyan::MaterialInstance* matl;

    Quad();
    ~Quad();
    void init(glm::vec2 pos, float width, float height);
    void draw();
};

struct Vertex
{
    glm::vec4 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 texCoord;
};

struct Triangle
{
    glm::vec3 m_vertices[3];
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd);
};

// SoA data oriented triangle mesh meant for imrpoving ray tracing procedure
struct TriangleArray 
{
    std::vector<glm::vec3> m_positionArray;
    std::vector<glm::vec3> m_normalArray;
    std::vector<glm::vec3> m_tangentArray;
    std::vector<glm::vec3> m_texCoordArray;
    u32 m_numVerts;
};

struct BoundingBox3f
{
    // in object space
    glm::vec4 pmin;
    glm::vec4 pmax;

    // debug rendering
    glm::vec3 vertices[8];
    glm::vec3 color;
    struct VertexArray* vertexArray;
    static Shader* shader;
    Cyan::MaterialInstance* matl;

    BoundingBox3f();

    void init();

    void draw(glm::mat4& mvp);
    void bound(const BoundingBox3f& aabb);
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
    void update();
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd);
};

