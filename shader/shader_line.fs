#version 450 core

in flat int vertexId;
out vec4 fragColor;

void main()
{
    fragColor = vertexId > 0 ? vec4(1.f, 0.f, 0.f, 1.f) : vec4(1.f, 1.f, 1.f, 1.f);
}