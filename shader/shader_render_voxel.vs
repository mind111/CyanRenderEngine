#version 450 core

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec2 vTexCoords;
out vec2 texCoords;
void main()
{
    texCoords = vTexCoords;
    gl_Position = vec4(vPos, 1.f);
}
