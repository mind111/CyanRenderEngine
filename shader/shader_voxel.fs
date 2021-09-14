#version 450 core

in VertexData
{
    vec3 normal;
    vec3 tangent;
    vec3 texCoords;
} VertexIn;

out vec4 fragColor;

void main()
{
    fragColor = vec4(0.8f, 0.8f, 0.8f, 1.f);
}