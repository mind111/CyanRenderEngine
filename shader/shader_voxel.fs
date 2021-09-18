#version 450 core

in VertexData
{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 texCoords;
} VertexIn;

out vec4 fragColor;

void main()
{
    float ndotl = max(dot(vec3(0.f, 1.f, 0.f), VertexIn.normal), 0.0); 
    vec3 n = (VertexIn.normal + vec3(1.f)) * 0.5f;
    fragColor = vec4(n, 1.f);
}