#pragma once

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"

#include "Shader.h"
#include "Material.h"

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

struct Triangle
{
    glm::vec4 m_vertices[3];
    float intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& transform);
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