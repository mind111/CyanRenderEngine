#version 450 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
out vec3 normal;
uniform mat4 s_model;
uniform mat4 s_view;
uniform mat4 s_projection;
void main() 
{
    normal = (transpose(inverse(s_model)) * vec4(vNormal, 1.f)).xyz;
    gl_Position = s_projection * s_view * s_model * vec4(vPos, 1.f);
}