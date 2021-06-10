#pragma once

#include "CyanAPI.h"
#include "Shader.h"
#include "Material.h"

// TODO: line segment rendered using a cylinder mesh

struct Line
{
    void init();
    Line& setTransforms(Uniform* view, Uniform* projection);
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