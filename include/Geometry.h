#include "CyanAPI.h"
#include "Shader.h"
#include "Material.h"

// TODO: line segment rendered using a cylinder mesh

struct Line
{
    glm::vec3 m_vertices[2];
    Cyan::MaterialInstance* m_matl;
    GLuint m_vbo, m_vao;
    Uniform* u_view;
    Uniform* u_projection;

    void init() 
    { 
        Shader* shader = Cyan::createShader("LineShader", "../../shader/shader_line.vs", "../../shader/shader_line.fs");
        m_matl = Cyan::createMaterial(shader)->createInstance();
        glCreateBuffers(1, &m_vbo);
        glNamedBufferData(m_vbo, sizeof(m_vertices), nullptr, GL_STATIC_DRAW);
        glCreateVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glEnableVertexArrayAttrib(m_vao, 0);
        glVertexAttribPointer(0, m_vertices[0].length(), GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    };

    Line& setTransforms(Uniform* view, Uniform* projection)
    {
        u_view = view;
        u_projection = projection;
        return *this;
    }

    Line& setVerts(glm::vec3 v0, glm::vec3 v1)
    {
        m_vertices[0] = v0;
        m_vertices[1] = v1;
        glNamedBufferData(m_vbo, sizeof(m_vertices), m_vertices, GL_STATIC_DRAW);
        return *this;
    }
    
    Line& setColor(glm::vec4 color)
    {
        m_matl->set("color", &color.r);
        return *this;
    }

    void draw() 
    { 
        auto ctx = Cyan::getCurrentGfxCtx();
        ctx->setShader(m_matl->m_template->m_shader);
        m_matl->bind();
        ctx->setUniform(u_view);
        ctx->setUniform(u_projection);
        glBindVertexArray(m_vao);
        glDrawArrays(GL_LINES, 0, 2);
        glBindVertexArray(0);
    };
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