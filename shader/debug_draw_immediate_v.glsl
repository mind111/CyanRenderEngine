#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;

out VSOutput
{
    vec4 color;
} vsOut;

uniform mat4 mvp;
uniform vec4 color;

void main() {
    vsOut.color = color;
    gl_Position = mvp * vec4(vertexPos, 1.f);
}
