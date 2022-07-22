#version 450 core
#define VIEW_SSBO_BINDING 0
layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

layout(std430, binding = 30) buffer DebugLineBuffer
{
    vec4 vertices[];
} lines;

void main() { 
    vec3 position = lines.vertices[gl_VertexID].xyz;
    gl_Position = viewSsbo.projection * viewSsbo.view * vec4(position, 1.f);
}