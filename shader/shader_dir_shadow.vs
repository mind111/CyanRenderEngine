#version 450 core
layout(location = 0) in vec3 vPos;
uniform mat4 model;
uniform mat4 lightView;
uniform mat4 lightProjection;
void main()
{
    gl_Position = lightProjection * lightView * model * vec4(vPos, 1.f);
}