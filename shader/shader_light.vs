#version 450 core

layout (location = 0) in vec3 vertexPos;

uniform mat4 s_model;
uniform mat4 s_view;
uniform mat4 s_projection;

void main() {
    gl_Position = s_projection * s_view * s_model * vec4(vertexPos, 1.f); 
}