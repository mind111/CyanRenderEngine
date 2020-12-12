#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec3 vertexTangent;
layout (location = 3) in vec2 textureUv;

out vec3 n;
out vec3 t;
out vec2 uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(vertexPos, 1.0f);
    mat4 normalXform = transpose(inverse(view * model)); 
    n = (normalXform * vec4(normalize(vertexNormal), 0.f)).xyz;
    n = normalize(vertexNormal);
    t = normalize(vertexTangent);
    uv = textureUv;
}