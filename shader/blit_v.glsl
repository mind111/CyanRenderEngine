#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 textureUv;

out vec2 uv;

void main() {
    uv = textureUv;
    gl_Position = vec4(vertexPos, 1.0f);
}