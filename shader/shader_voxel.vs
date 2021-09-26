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
    vec3 fragmentWorldPos;
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
    vec4 worldPos = model * vec4(vPos.xyz, 1.0f);
    VertexOut.fragmentWorldPos = worldPos.xyz;
    gl_Position = worldPos;
}