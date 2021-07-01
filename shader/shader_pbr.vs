#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec2 textureUv;
layout (location = 3) in vec3 vertexTangent;

out vec3 n;
out vec3 t;
out vec2 uv;
out vec3 fragmentPos;

uniform mat4 s_model;
uniform mat4 s_view;
uniform mat4 s_projection;

void main() {
    fragmentPos = (s_view * s_model * vec4(vertexPos, 1.f)).xyz; 
    gl_Position = s_projection * s_view * s_model * vec4(vertexPos, 1.0f);
    mat4 normalTransform = transpose(inverse(s_view * s_model)); 
    // Transform normals to camera space
    n = (normalTransform * vec4(vertexNormal, 0.f)).xyz;
    n = normalize(n);
    // Transform tangents to camera space
    t = (s_view * s_model * vec4(vertexTangent, 0.f)).xyz;
    t = normalize(t);
    uv = textureUv;
}