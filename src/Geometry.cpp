#include "Geometry.h"

void Line::init() 
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

Line& Line::setTransforms(Uniform* view, Uniform* projection)
{
    u_view = view;
    u_projection = projection;
    return *this;
}

Line& Line::setVerts(glm::vec3 v0, glm::vec3 v1)
{
    m_vertices[0] = v0;
    m_vertices[1] = v1;
    glNamedBufferData(m_vbo, sizeof(m_vertices), m_vertices, GL_STATIC_DRAW);
    return *this;
}

Line& Line::setColor(glm::vec4 color)
{
    m_matl->set("color", &color.r);
    return *this;
}

void Line::draw() 
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

void Quad::init(glm::vec2 pos, float width, float height)
{
    m_pos = pos;
    w = width;
    h = height;        
    m_vertices[0] = pos + glm::vec2(-0.5f * w,  0.5 * h);
    m_vertices[1] = pos + glm::vec2(-0.5f * w, -0.5 * h);
    m_vertices[2] = pos + glm::vec2( 0.5f * w, -0.5 * h);
    m_vertices[3] = pos + glm::vec2(-0.5f * w,  0.5 * h);
    m_vertices[4] = pos + glm::vec2( 0.5f * w, -0.5 * h);
    m_vertices[5] = pos + glm::vec2( 0.5f * w,  0.5 * h);

    // TODO: this is bad, Quad class should only need static shader
    Shader* shader = Cyan::createShader("QuadShader", "../../shader/shader_quad.vs", "../../shader/shader_quad.fs");
    m_matl = Cyan::createMaterial(shader)->createInstance();

    glCreateBuffers(1, &m_vbo);
    glNamedBufferData(m_vbo, sizeof(m_vertices), m_vertices, GL_STATIC_DRAW);
    glCreateVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexAttribPointer(0, m_vertices[0].length(), GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Quad::draw()
{
    Cyan::getCurrentGfxCtx()->setShader(m_matl->m_template->m_shader);
    m_matl->bind();
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void BufferVisualizer::draw()
{
    for (auto& chunk : m_chunks)
    {
        chunk.m_quad.draw();
    }
}

void BufferVisualizer::init(UniformBuffer* buffer, glm::vec2 pos, float width, float height)
{
    m_width = width;
    m_height = height;
    m_pos = pos;
    m_buffer = buffer;

    u32 cachedCurrentPos = m_buffer->m_pos;
    m_buffer->reset();
    u32 index = 0;
    f32 offsetX = m_pos.x - 0.5f * m_width;

    for (;;) 
    {
        UniformHandle handle = m_buffer->readU32();
        BREAK_WHEN(handle == UniformBuffer::End)
        Uniform* uniform = Cyan::getUniform(handle);
        Chunk chunk = { };

        auto initChunk = [&](glm::vec3 color)
        {
            chunk.m_color = color;
            f32 chunkWidth = (f32)uniform->getSize() / m_buffer->m_size * m_width;
            chunk.m_quad.init(glm::vec2(offsetX + 0.6f * chunkWidth, m_pos.y), chunkWidth, m_height);
            chunk.m_quad.m_matl->set("color", (void*)&chunk.m_color[0]);
            chunk.m_quad.m_matl->m_uniformBuffer->debugPrint();
            auto ignored = m_buffer->read(uniform->getSize());
            offsetX += chunkWidth * 1.1f;
        };

        switch (uniform->m_type)
        {
            case Uniform::Type::u_int:
            {
                initChunk(glm::vec3(0.f, 1.f, 1.f));
                break;
            }
            case Uniform::Type::u_float:
            {
                initChunk(glm::vec3(0.980f, 0.502f, 0.447f));
                break;
            }
            case Uniform::Type::u_vec3:
            {
                initChunk(glm::vec3(0.2f, 0.4f, 0.3f));
                break;
            }
            default:
                break;
        }
        index++;
        m_chunks.emplace_back(chunk);
    }
    m_buffer->reset(cachedCurrentPos);
}