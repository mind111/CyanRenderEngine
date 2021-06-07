#version 450 core

layout (location = 0) in vec4 vertexPos;

uniform mat4 s_view;
uniform mat4 s_projection;

void main() {
    gl_Position = s_projection * s_view * vec4(vertexPos.xyz, 1.f);
}