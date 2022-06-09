#version 450 core

layout(location = 0) in vec3 vPos;
out vec4 vPosCS; 

#define VIEW_SSBO_BINDING 0
#define TRANSFORM_SSBO_BINDING 3

layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

layout(std430, binding = TRANSFORM_SSBO_BINDING) buffer TransformShaderStorageBuffer
{
    mat4 models[];
} transformSsbo;

uniform int transformIndex;

void main()
{
    mat4 model = transformSsbo.models[transformIndex];
    vPosCS = viewSsbo.projection * viewSsbo.view * model * vec4(vPos, 1.f);
    gl_Position = vPosCS;
}
