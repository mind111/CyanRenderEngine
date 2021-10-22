#version 450 core
layout (location = 0) in vec3 vPos;
out vec3 vn;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    vn = (inverse(transpose(model)) * vec4(vNormal, 0.f)).xyz;
    gl_Position = projection * view * model * vec4(vPos, 1.f);
}