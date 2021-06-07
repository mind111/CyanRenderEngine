#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 texCoords;

out vec2 uv;

void main()
{
    uv = texCoords;
    gl_Position = vec4(vertexPos, 1.f); 
}