#version 450 core
layout(location = 0) in vec3 vPos;
out vec3 vPosCS; 
uniform mat4 model;
uniform mat4 lightView;
uniform mat4 lightProjection;
void main()
{
    vPosCS = (lightProjection * lightView * model * vec4(vPos, 1.f)).xyz;
    gl_Position = vec4(vPosCS, 1.f);
}