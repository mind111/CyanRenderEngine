#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 textureCoord;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

out vec3 fragNormal;
out vec2 textureUV;
out vec3 fragTangent;
out vec3 fragBitangent;
out vec3 fragPosView;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    fragNormal = (view * model * vec4(normal, 0.0)).xyz;
    fragTangent = (view * model * vec4(tangent, 0.0)).xyz;
    fragBitangent = (view * model * vec4(bitangent, 0.0)).xyz;
    fragPosView = (view * model * vec4(vertexPos, 1.0)).xyz;
    textureUV = textureCoord;
    gl_Position = projection * view * model * vec4(vertexPos, 1.0);
}