#version 450 core

layout(location = 0) vec3 vPos;

uniform mat4 model;
uniform mat4 lightView;

void main()
{
    gl_Position = lightView * model * vec4(vPos, 1.f);
}