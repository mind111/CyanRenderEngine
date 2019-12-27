#version 450 core

layout (location = 0) in vec3 vertexPosModel;
layout (location = 1) in vec3 vertexColor;
out vec4 color;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(vertexPosModel, 1.0f);
    color = vec4(vertexColor, 0.2f);
}