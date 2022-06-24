#version 450 core
layout (location = 0) in vec3 vPos; 
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec4 vTangent;

#define VIEW_SSBO_BINDING 0
#define TRANSFORM_SSBO_BINDING 3

out vec3 normalWorld;
out vec3 tangentWorld;

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
    normalWorld = (inverse(transpose(model)) * vec4(vNormal, 0.f)).xyz;
    // todo: handedness of tangent
    tangentWorld = normalize((model * vec4(vTangent.xyz, 0.f)).xyz);
    gl_Position = viewSsbo.projection * viewSsbo.view * model * vec4(vPos, 1.f);
}