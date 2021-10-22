#include "Geometry.h"
#include "CyanAPI.h"
#include "VertexArray.h"

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

Line& Line::setViewProjection(Uniform* view, Uniform* projection)
{
    u_view = view;
    u_projection = projection;
    return *this;
}

Line& Line::setModel(glm::mat4& mat)
{
    m_matl->set("lineModel", reinterpret_cast<void*>(&mat[0][0]));
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

void Line2D::init()
{
    Shader* shader = Cyan::createShader("Line2DShader", "../../shader/shader_line2d.vs", "../../shader/shader_line2d.fs");
    m_matl = Cyan::createMaterial(shader)->createInstance();
    auto vb = Cyan::createVertexBuffer(m_vertices, sizeof(m_vertices), 3 * sizeof(f32), 2);
    vb->addVertexAttrb({VertexAttrib::DataType::Float, 3, 3 * sizeof(f32), 0, nullptr});
    m_vertexArray = Cyan::createVertexArray(vb);
    m_vertexArray->init();
}

void Line2D::setVerts(glm::vec3& v0, glm::vec3& v1)
{
    m_vertices[0] = v0;
    m_vertices[1] = v1;
    glNamedBufferSubData(m_vertexArray->m_vertexBuffer->m_vbo, 0, sizeof(m_vertices), m_vertices);
}

void Line2D::setColor(glm::vec4& color)
{
    m_color = color;
}

void Line2D::draw()
{
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setDepthControl(Cyan::DepthControl::kDisable);
    ctx->setShader(m_matl->m_template->m_shader);
    m_matl->set("color", &m_color.r);
    m_matl->bind();
    ctx->setVertexArray(m_vertexArray);
    ctx->setPrimitiveType(Cyan::PrimitiveType::Line);
    ctx->drawIndexAuto(2);
    ctx->setPrimitiveType(Cyan::PrimitiveType::TriangleList);
    ctx->setDepthControl(Cyan::DepthControl::kEnable);
    // glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // glDrawArrays(GL_LINE, 0, 2);
    // glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Quad::Quad()
    : m_matl(0)
{
    // do nothing
}

Quad::~Quad()
{

}

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
            glm::vec4 color4(chunk.m_color, 1.f);
            chunk.m_quad.m_matl->set("color", (void*)&color4.r);
            // chunk.m_quad.m_matl->m_uniformBuffer->debugPrint();
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

glm::vec3 vec4ToVec3(const glm::vec4& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

u32 indices[24] = {
    0, 1, 
    1, 2, 
    2, 3, 
    0, 3, 
    1, 4, 
    4, 5, 
    5, 2,
    5, 6,
    4, 7,
    6, 7,
    3, 6,
    0, 7
};

// setup for debug rendering
void BoundingBox3f::init()
{
    glm::vec3 pMin = vec4ToVec3(m_pMin);
    glm::vec3 pMax = vec4ToVec3(m_pMax);
    glm::vec3 dim = pMax - pMin;

    // compute all verts
    m_vertices[0] = pMin;
    m_vertices[1] = m_vertices[0] + glm::vec3(dim.x, 0.f, 0.f);
    m_vertices[2] = m_vertices[1] + glm::vec3(0.f, dim.y, 0.f);
    m_vertices[3] = m_vertices[2] + glm::vec3(-dim.x, 0.f, 0.f);

    // dim.z < 0 becasue m_pMin.z > 0 while m_pMax.z < 0 
    m_vertices[4] = m_vertices[1] + glm::vec3(0.f, 0.f, dim.z);
    m_vertices[5] = m_vertices[2] + glm::vec3(0.f, 0.f, dim.z);
    m_vertices[6] = m_vertices[3] + glm::vec3(0.f, 0.f, dim.z);
    m_vertices[7] = m_vertices[0] + glm::vec3(0.f, 0.f, dim.z);

    auto vb = Cyan::createVertexBuffer(reinterpret_cast<void*>(m_vertices), sizeof(m_vertices), sizeof(glm::vec3), 8u);
    vb->m_vertexAttribs.push_back({
        VertexAttrib::DataType::Float, 3, sizeof(glm::vec3), 0u, nullptr
    });
    m_vertexArray = Cyan::createVertexArray(vb);
    glCreateBuffers(1, &m_vertexArray->m_ibo);
    glBindVertexArray(m_vertexArray->m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_vertexArray->m_ibo);
    glNamedBufferData(m_vertexArray->m_ibo, sizeof(indices),
                        reinterpret_cast<const void*>(indices), GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m_vertexArray->init();
    Shader* shader = Cyan::createShader("LineShader", "../../shader/shader_line.vs", "../../shader/shader_line.fs");
    m_matl = Cyan::createMaterial(shader)->createInstance();
    isValid = true;
    m_color = glm::vec3(1.0, 0.8, 0.f);
}

void BoundingBox3f::setModel(glm::mat4& model)
{
    m_matl->set("lineModel", reinterpret_cast<void*>(&model[0]));
}

void BoundingBox3f::setViewProjection(Uniform* view, Uniform* projection)
{
    u_view = view;
    u_projection = projection;
}

void BoundingBox3f::draw()
{
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setShader(Cyan::createShader("LineShader", "../../shader_line.vs", "../../shader_line.fs"));
    ctx->setVertexArray(m_vertexArray);
    m_matl->set("color", &m_color.r);
    m_matl->bind();
    ctx->setUniform(u_view);
    ctx->setUniform(u_projection);
    ctx->setPrimitiveType(Cyan::PrimitiveType::Line);
    ctx->drawIndex(sizeof(indices) / sizeof(u32));
}

void BoundingBox3f::computeVerts()
{
    glm::vec3 pMin = vec4ToVec3(m_pMin);
    glm::vec3 pMax = vec4ToVec3(m_pMax);
    glm::vec3 dim = pMax - pMin;

    // compute all verts
    m_vertices[0] = pMin;
    m_vertices[1] = m_vertices[0] + glm::vec3(dim.x, 0.f, 0.f);
    m_vertices[2] = m_vertices[1] + glm::vec3(0.f, dim.y, 0.f);
    m_vertices[3] = m_vertices[2] + glm::vec3(-dim.x, 0.f, 0.f);

    // dim.z < 0 becasue m_pMin.z > 0 while m_pMax.z < 0 
    m_vertices[4] = m_vertices[1] + glm::vec3(0.f, 0.f, dim.z);
    m_vertices[5] = m_vertices[2] + glm::vec3(0.f, 0.f, dim.z);
    m_vertices[6] = m_vertices[3] + glm::vec3(0.f, 0.f, dim.z);
    m_vertices[7] = m_vertices[0] + glm::vec3(0.f, 0.f, dim.z);

    glNamedBufferData(m_vertexArray->m_vertexBuffer->m_vbo, sizeof(m_vertices), m_vertices, GL_STATIC_DRAW);
}

void BoundingBox3f::resetBound()
{
    m_pMin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    m_pMax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
}

void BoundingBox3f::bound(const BoundingBox3f& aabb) 
{
    m_pMin.x = Min(m_pMin.x, aabb.m_pMin.x);
    m_pMin.y = Min(m_pMin.y, aabb.m_pMin.y);
    m_pMin.z = Min(m_pMin.z, aabb.m_pMin.z);

    m_pMax.x = Max(m_pMax.x, aabb.m_pMax.x);
    m_pMax.y = Max(m_pMax.y, aabb.m_pMax.y);
    m_pMax.z = Max(m_pMax.z, aabb.m_pMax.z);
}

void BoundingBox3f::bound(glm::vec3& v3)
{
    m_pMin.x = Min(m_pMin.x, v3.x);
    m_pMin.y = Min(m_pMin.y, v3.y);
    m_pMin.z = Min(m_pMin.z, v3.z);
    m_pMax.x = Max(m_pMax.x, v3.x);
    m_pMax.y = Max(m_pMax.y, v3.y);
    m_pMax.z = Max(m_pMax.z, v3.z);
}

void BoundingBox3f::bound(glm::vec4& v4)
{
    m_pMin.x = Min(m_pMin.x, v4.x);
    m_pMin.y = Min(m_pMin.y, v4.y);
    m_pMin.z = Min(m_pMin.z, v4.z);
    m_pMax.x = Max(m_pMax.x, v4.x);
    m_pMax.y = Max(m_pMax.y, v4.y);
    m_pMax.z = Max(m_pMax.z, v4.z);
}

inline f32 ffmin(f32 a, f32 b) { return a < b ? a : b;}
inline f32 ffmax(f32 a, f32 b) { return a > b ? a : b;}

// do this computation in view space
float BoundingBox3f::intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& transform)
{
    f32 tMin, tMax;
    glm::vec4 pMinView = transform * m_pMin;
    glm::vec4 pMaxView = transform * m_pMax;
    glm::vec3 min = glm::vec3(pMinView.x, pMinView.y, pMinView.z);
    glm::vec3 max = glm::vec3(pMaxView.x, pMaxView.y, pMaxView.z);

    // x-min, x-max
    f32 txMin = ffmin((min.x - ro.x) / rd.x, (max.x - ro.x) / rd.x);
    f32 txMax = ffmax((min.x - ro.x) / rd.x, (max.x - ro.x) / rd.x);
    // y-min, y-max
    f32 tyMin = ffmin((min.y - ro.y) / rd.y, (max.y - ro.y) / rd.y);
    f32 tyMax = ffmax((min.y - ro.y) / rd.y, (max.y - ro.y) / rd.y);
    // z-min, z-max
    f32 tzMin = ffmin((min.z - ro.z) / rd.z, (max.z - ro.z) / rd.z);
    f32 tzMax = ffmax((min.z - ro.z) / rd.z, (max.z - ro.z) / rd.z);
    tMin = ffmax(ffmax(txMin, tyMin), tzMin);
    tMax = ffmin(ffmin(txMax, tyMax), tzMax);
    if (tMin <= tMax)
    {
        return tMin;
    }
    return -1.0;
}

// taken from https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
float Triangle::intersectRay(const glm::vec3& ro, const glm::vec3& rd, const glm::mat4& transform)
{
    const float EPSILON = 0.0000001;
    glm::vec4 v0_view = transform * m_vertices[0]; 
    glm::vec4 v1_view = transform * m_vertices[1];
    glm::vec4 v2_view = transform * m_vertices[2];

    glm::vec3& v0 = glm::vec3(v0_view.x, v0_view.y, v0_view.z);
    glm::vec3& v1 = glm::vec3(v1_view.x, v1_view.y, v1_view.z);
    glm::vec3& v2 = glm::vec3(v2_view.x, v2_view.y, v2_view.z);
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h, s, q;
    float a,f,u,v;

    h = glm::cross(rd, edge2);
    a = glm::dot(edge1, h);
    if (a > -EPSILON && a < EPSILON)
    {
        return -1.0;
    }
    f = 1.0f / a;
    s = ro - v0;
    u = f * dot(s, h);
    if (u < 0.0 || u > 1.0)
    {
        return -1.0;
    }
    q = glm::cross(s, edge1);
    v = f * dot(rd, q);
    if (v < 0.0 || u + v > 1.0)
    {
        return -1.0;
    }
    float t = f * glm::dot(edge2, q);
    if (t > EPSILON)
    {
        return t;
    }
    return -1.0f;
}
