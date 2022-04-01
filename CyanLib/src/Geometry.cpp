#include "Geometry.h"
#include "CyanAPI.h"
#include "VertexArray.h"
#include "MathUtils.h"

PointGroup::PointGroup(u32 size) 
    : kMaxNumPoints(size),
    m_numPoints(0),
    m_shader(nullptr),
    matl(nullptr)
{
    m_points.resize(kMaxNumPoints);
    VertexBuffer* vb = Cyan::createVertexBuffer(m_points.data(), sizeof(Point) * m_points.size(), sizeof(Point), m_points.size());
    vb->addVertexAttrib({VertexAttrib::DataType::Float, 3, sizeof(Point), 0 });
    m_vertexArray = Cyan::createVertexArray(vb);
    m_vertexArray->init();
    m_shader = Cyan::createShader("LineShader", "../../shader/shader_line.vs", "../../shader/shader_line.fs");
    matl = Cyan::createMaterial(m_shader)->createInstance();
    glPointSize(7.f);
}

void PointGroup::push(glm::vec3& position)
{
    CYAN_ASSERT(m_numPoints < kMaxNumPoints, "Pushing too many points into a point group!\n");
    m_points[m_numPoints++] = Point{position};
}

void PointGroup::reset()
{
    m_numPoints = 0;
}

void PointGroup::clear()
{
    m_points.clear();
    m_numPoints = 0;
}

void PointGroup::setColor(const glm::vec4& color)
{
    matl->set("color", &color.x);
}

void PointGroup::draw(glm::mat4& mvp)
{
    glNamedBufferSubData(m_vertexArray->m_vertexBuffer->m_vbo, 0, sizeof(m_points[0]) * m_points.size(), m_points.data());

    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setShader(m_shader);
    matl->set("mvp", &mvp[0]);
    matl->bind();
    ctx->setVertexArray(m_vertexArray);
    ctx->setPrimitiveType(Cyan::PrimitiveType::Points);
    ctx->drawIndexAuto(m_vertexArray->numVerts());
}

void Line::init() 
{ 
    Shader* shader = Cyan::createShader("LineShader", "../../shader/shader_line.vs", "../../shader/shader_line.fs");
    matl = Cyan::createMaterial(shader)->createInstance();
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

Line& Line::setVertices(glm::vec3 v0, glm::vec3 v1)
{
    m_vertices[0] = v0;
    m_vertices[1] = v1;
    glNamedBufferData(m_vbo, sizeof(m_vertices), m_vertices, GL_STATIC_DRAW);
    return *this;
}

Line& Line::setColor(glm::vec4 color)
{
    matl->set("color", &color.r);
    return *this;
}

void Line::draw(glm::mat4& mvp) 
{ 
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setShader(matl->m_template->m_shader);
    matl->set("mvp", &mvp[0]);
    matl->bind();
    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
};

void Line2D::init()
{
    Shader* shader = Cyan::createShader("Line2DShader", "../../shader/shader_line2d.vs", "../../shader/shader_line2d.fs");
    matl = Cyan::createMaterial(shader)->createInstance();
    auto vb = Cyan::createVertexBuffer(m_vertices, sizeof(m_vertices), 3 * sizeof(f32), 2);
    vb->addVertexAttrib({VertexAttrib::DataType::Float, 3, 3 * sizeof(f32), 0 });
    m_vertexArray = Cyan::createVertexArray(vb);
    m_vertexArray->init();
}

void Line2D::setVerts(glm::vec3& v0, glm::vec3& v1)
{
    m_vertices[0] = v0;
    m_vertices[1] = v1;
    glNamedBufferSubData(m_vertexArray->m_vertexBuffer->m_vbo, 0, sizeof(m_vertices), m_vertices);
}

void Line2D::setColor(const glm::vec4& color)
{
    m_color = color;
}

void Line2D::draw()
{
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setDepthControl(Cyan::DepthControl::kDisable);
    ctx->setShader(matl->m_template->m_shader);
    matl->set("color", &m_color.r);
    matl->bind();
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
    : matl(0)
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
    matl = Cyan::createMaterial(shader)->createInstance();

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
    Cyan::getCurrentGfxCtx()->setShader(matl->m_template->m_shader);
    matl->bind();
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
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

Shader* BoundingBox3f::shader = nullptr;

BoundingBox3f::BoundingBox3f()
{
    pmin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    pmax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
    vertexArray = nullptr;
    if (!shader)
    {
        shader = Cyan::createShader("LineShader", SHADER_SOURCE_PATH "shader_line.vs", SHADER_SOURCE_PATH "shader_line.fs");
    }
}

// setup for debug rendering
void BoundingBox3f::init()
{
    auto vb = Cyan::createVertexBuffer(reinterpret_cast<void*>(vertices), sizeof(vertices), sizeof(glm::vec3), 8u);
    vb->m_vertexAttribs.push_back({
        VertexAttrib::DataType::Float, 3, sizeof(glm::vec3), 0u
    });
    vertexArray = Cyan::createVertexArray(vb);
    glCreateBuffers(1, &vertexArray->m_ibo);
    glBindVertexArray(vertexArray->m_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexArray->m_ibo);
    glNamedBufferData(vertexArray->m_ibo, sizeof(indices),
                        reinterpret_cast<const void*>(indices), GL_STATIC_DRAW);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    vertexArray->init();
    matl = Cyan::createMaterial(shader)->createInstance();
    color = glm::vec3(1.0, 0.8, 0.f);
}

void BoundingBox3f::draw(glm::mat4& mvp)
{
    auto ctx = Cyan::getCurrentGfxCtx();
    ctx->setShader(shader);
    matl->set("color", &color.r);
    matl->set("mvp", &mvp[0]);
    matl->bind();

    ctx->setPrimitiveType(Cyan::PrimitiveType::Line);
    ctx->setVertexArray(vertexArray);
    ctx->drawIndex(sizeof(indices) / sizeof(u32));
}

void BoundingBox3f::update()
{
    glm::vec3 pMin = Cyan::vec4ToVec3(pmin);
    glm::vec3 pMax = Cyan::vec4ToVec3(pmax);
    glm::vec3 dim = pMax - pMin;

    // update vertices
    vertices[0] = pMin;
    vertices[1] = vertices[0] + glm::vec3(dim.x, 0.f, 0.f);
    vertices[2] = vertices[1] + glm::vec3(0.f, dim.y, 0.f);
    vertices[3] = vertices[2] + glm::vec3(-dim.x, 0.f, 0.f);

    // dim.z < 0 becasue m_pMin.z > 0 while m_pMax.z < 0 
    vertices[4] = vertices[1] + glm::vec3(0.f, 0.f, dim.z);
    vertices[5] = vertices[2] + glm::vec3(0.f, 0.f, dim.z);
    vertices[6] = vertices[3] + glm::vec3(0.f, 0.f, dim.z);
    vertices[7] = vertices[0] + glm::vec3(0.f, 0.f, dim.z);

    glNamedBufferSubData(vertexArray->m_vertexBuffer->m_vbo, 0, sizeof(vertices), vertices);
}

void BoundingBox3f::reset()
{
    pmin = glm::vec4(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
    pmax = glm::vec4(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);
    for (u32 i = 0; i < 8; ++i)
    {
        vertices[i] = glm::vec3(0.f);
    }
    glNamedBufferSubData(vertexArray->m_vertexBuffer->m_vbo, 0, sizeof(vertices), vertices);
}

void BoundingBox3f::bound(const BoundingBox3f& aabb) 
{
    pmin.x = Min(pmin.x, aabb.pmin.x);
    pmin.y = Min(pmin.y, aabb.pmin.y);
    pmin.z = Min(pmin.z, aabb.pmin.z);

    pmax.x = Max(pmax.x, aabb.pmax.x);
    pmax.y = Max(pmax.y, aabb.pmax.y);
    pmax.z = Max(pmax.z, aabb.pmax.z);
}

void BoundingBox3f::bound(const glm::vec3& v3)
{
    pmin.x = Min(pmin.x, v3.x);
    pmin.y = Min(pmin.y, v3.y);
    pmin.z = Min(pmin.z, v3.z);
    pmax.x = Max(pmax.x, v3.x);
    pmax.y = Max(pmax.y, v3.y);
    pmax.z = Max(pmax.z, v3.z);
}

void BoundingBox3f::bound(const glm::vec4& v4)
{
    pmin.x = Min(pmin.x, v4.x);
    pmin.y = Min(pmin.y, v4.y);
    pmin.z = Min(pmin.z, v4.z);
    pmax.x = Max(pmax.x, v4.x);
    pmax.y = Max(pmax.y, v4.y);
    pmax.z = Max(pmax.z, v4.z);
}

void BoundingBox3f::bound(const Triangle& tri)
{
    bound(tri.m_vertices[0]);
    bound(tri.m_vertices[1]);
    bound(tri.m_vertices[2]);
}

inline f32 ffmin(f32 a, f32 b) { return a < b ? a : b;}
inline f32 ffmax(f32 a, f32 b) { return a > b ? a : b;}

// do this computation in view space
float BoundingBox3f::intersectRay(const glm::vec3& ro, const glm::vec3& rd)
{
    f32 tMin, tMax;
    glm::vec3 min = Cyan::vec4ToVec3(pmin);
    glm::vec3 max = Cyan::vec4ToVec3(pmax);

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
        return tMin > 0.f ? tMin : tMax;
    }
    return -1.0;
}

// taken from https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
/* 
    Note(Min): It seems using glm::dot() is quite slow here; Implementing own math library should be beneficial 
*/
float Triangle::intersectRay(const glm::vec3& roObjectSpace, const glm::vec3& rdObjectSpace)
{
    const float EPSILON = 0.0000001;
    glm::vec3 edge1 = m_vertices[1] - m_vertices[0];
    glm::vec3 edge2 = m_vertices[2] - m_vertices[0];
    glm::vec3 h, s, q;
    float a,f,u,v;

    h = glm::cross(rdObjectSpace, edge2);
    // a = glm::dot(edge1, h);
    a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;
    if (fabs(a) < EPSILON)
    {
        return -1.0;
    }
    f = 1.0f / a;
    s = roObjectSpace - m_vertices[0];
    // u = f * dot(s, h);
    u = f * (s.x*h.x + s.y*h.y + s.z*h.z);
    if (u < 0.0 || u > 1.0)
    {
        return -1.0;
    }
    q = glm::cross(s, edge1);
    // v = f * dot(rdObjectSpace, q);
    v = f * (rdObjectSpace.x*q.x + rdObjectSpace.y*q.y + rdObjectSpace.z*q.z);
    if (v < 0.0 || u + v > 1.0)
    {
        return -1.0;
    }
    float t = f * glm::dot(edge2, q);
    // hit
    if (t > EPSILON)
    {
        return t;
    }
    return -1.0f;
}
