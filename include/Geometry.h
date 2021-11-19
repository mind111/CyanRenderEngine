#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "VertexArray.h"
#include "Shader.h"
#include "Material.h"

struct Point
{
    glm::vec3 m_position;
};

struct PointGroup
{
    PointGroup(u32 size);
    void push(glm::vec3& position);
    void draw();
    void reset();
    void clear();
    void setViewProjection(Uniform* view, Uniform* projection);
    void setColor(glm::vec4& color);

    std::vector<Point> m_points;
    Uniform* u_view;
    Uniform* u_projection;
    u32 kMaxNumPoints;
    u32 m_numPoints;
    Shader* m_shader;
    Cyan::MaterialInstance* m_matl;
    VertexArray* m_vertexArray;
};

// TODO: line segment rendered using a cylinder mesh
struct Line
{
    void init();
    Line& setViewProjection(Uniform* view, Uniform* projection);
    Line& setModel(glm::mat4& model);
    Line& setVerts(glm::vec3 v0, glm::vec3 v1);
    Line& setColor(glm::vec4 color);
    void draw();

    glm::vec3 m_vertices[2];
    Cyan::MaterialInstance* m_matl;
    GLuint m_vbo, m_vao;
    Uniform* u_view;
    Uniform* u_projection;
};

struct Line2D
{
    Line2D(glm::vec3& v0, glm::vec3& v1) 
        : m_vertices{ v0, v1 }
    {
        init();
    }
    void init();
    void setVerts(glm::vec3& v0, glm::vec3& v1);
    void setColor(glm::vec4& color);
    void draw();

    glm::vec3 m_vertices[2];
    glm::vec4 m_color;
    GLuint m_vbo;
    VertexArray* m_vertexArray;
    Cyan::MaterialInstance* m_matl;
};

struct Quad
{
    glm::vec2 m_pos; // [-1.f, 1.f]
    float w, h;      // [0.f, 2.f]
    glm::vec2 m_vertices[6];
    GLuint m_vao;
    GLuint m_vbo;
    Cyan::MaterialInstance* m_matl;

    Quad();
    ~Quad();
    void init(glm::vec2 pos, float width, float height);
    void draw();
};

struct Chunk
{
    Quad m_quad;
    glm::vec3 m_color;
};

// TODO: Draw a wire frame boundry around the visualizer
struct BufferVisualizer
{
    UniformBuffer* m_buffer;
    std::vector<Chunk> m_chunks;
    glm::vec2 m_pos;
    float m_width;
    float m_height;

    void init(UniformBuffer* buffer, glm::vec2 pos, float width, float height);
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
    Vertex m_vertices[3];
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& transform);
};

// SoA data oriented triangle mesh meant for imrpoving ray tracing procedure
struct TriangleArray 
{
    std::vector<glm::vec4> m_positionArray;
    std::vector<glm::vec3> m_normalArray;
    std::vector<glm::vec3> m_tangentArray;
    std::vector<glm::vec3> m_texCoordArray;
    u32 m_numVerts;
};

struct BoundingBox3f
{
    // pmin & pmax in object space
    glm::vec4 m_pMin;
    glm::vec4 m_pMax;

    // debug rendering
    glm::vec3 m_vertices[8];
    glm::vec3 m_color;
    struct VertexArray* m_vertexArray;
    GLuint m_ibo;
    bool isValid;

    // TODO: cleanup
    // transform uniforms
    Uniform* u_view;
    Uniform* u_projection;

    Cyan::MaterialInstance* m_matl;

    BoundingBox3f()
    {
        m_pMin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
        m_pMax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
        m_vertexArray = nullptr;
        m_ibo = -1;
        isValid = false;
    } 

    void init();
    void setModel(glm::mat4& model);
    void setViewProjection(Uniform* view, Uniform* projection);
    void computeVerts();
    void draw();
    void resetBound();
    void bound(const BoundingBox3f& aabb);
    void bound(glm::vec3& v3);
    void bound(glm::vec4& v4);
    float BoundingBox3f::intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& transform);
};

struct BVHNode
{
    BVHNode* m_parent;
    BVHNode* m_child[2];
    BoundingBox3f m_aabb;
};