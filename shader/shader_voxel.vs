#version 450 core

in vec3 vPos;
in vec3 vNormal;
in vec3 vTangent;
in vec2 vTexCoords;

out VertexData
{
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec3 texCoords;
} VertexOut;

uniform mat4 model;

void main()
{
    gl_Position = model * glm::vec4(vPos, 1.0f);
}