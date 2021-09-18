#version 450 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec4 vTangent;
layout (location = 3) in vec2 vTexCoords;

out VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
} VertexOut;

uniform mat4 model;
// aabb need to be transformed to world space
uniform vec3 aabbMin;
uniform vec3 aabbMax;

mat4 ortho(float l, float r, float b, float t, float n, float f)
{
    vec4 col0 = vec4(2.0f/(r - l), 0.f, 0.f, 0.f);
    vec4 col1 = vec4(0.f, 2.f/(t - b),  0.f,  0.f);
    vec4 col2 = vec4(0.f, 0.f, -2.f/(f - n), 0.f);
    vec4 col3 = vec4(-(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1.0f);
    return mat4(col0, col1, col2, col3);
}

void main()
{
    VertexOut.position = vPos;
    VertexOut.normal = vNormal;
    VertexOut.tangent = vTangent;
    VertexOut.texCoords = vTexCoords;

    mat4 proj = ortho(aabbMin.x, aabbMax.x, aabbMin.y, aabbMax.y, aabbMin.z, aabbMax.z);
    float aspect = (aabbMax.x - aabbMin.x) / (aabbMax.y - aabbMin.y);
    vec4 vPosWorld = model * vec4(vPos, 1.f);
    vPosWorld.x = 2.0 * (vPosWorld.x - aabbMin.x) / (aabbMax.x - aabbMin.x) - 1.0;
    vPosWorld.x /= (16.0 / 9.0);
    vPosWorld.y = 2.0 * (vPosWorld.y - aabbMin.y) / (aabbMax.x - aabbMin.x) - 1.0;
    vPosWorld.z = abs(vPosWorld.z - aabbMax.z) / (aabbMin.z - aabbMax.z);
    // gl_Position = proj * model * vec4(vPos, 1.0f);
    gl_Position = vec4(vPosWorld.xyz, 1.0f);
}