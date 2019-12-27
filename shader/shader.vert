#version 450 core

layout (location = 0) in vec3 position_model;
layout (location = 1) in vec3 vertex_normal;
layout (location = 2) in vec3 vertex_tangent;
layout (location = 3) in vec2 texture_uv;

out float fragment_depth;
out vec3 fragment_normal;
out vec3 fragment_tangent;
out vec2 fragment_texture_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(position_model, 1.0f);
    fragment_normal = vertex_normal;
    fragment_tangent = vertex_tangent;
    fragment_depth = gl_Position.z / gl_Position.w; 
    fragment_texture_uv = texture_uv;
}