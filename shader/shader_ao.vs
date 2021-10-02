#version 450 core
layout (location = 0) vec3 vPos;
layout (location = 1) vec2 vTexCoords;
out vec2 texCoords;
void main()
{
    gl_Position = vec4(vPos, 1.f);
}