#version 450 core
layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
out vec3 normal;
uniform mat4 s_model;
uniform mat4 s_view;
uniform mat4 s_projection;

layout(std430, binding = 0) buffer GlobalDrawData
{
    mat4  view;
    mat4  projection;
	mat4  sunLightView;
	mat4  sunShadowProjections[4];
    int   numDirLights;
    int   numPointLights;
    float m_ssao;
    float dummy;
} gDrawData;

layout(std430, binding = 3) buffer InstanceTransformData
{
    mat4 models[];
} gInstanceTransforms;
uniform int transformIndex;
uniform mat4 s_model;

void main() 
{
    normal = (transpose(inverse(s_model)) * vec4(vNormal, 1.f)).xyz;
    gl_Position = s_projection * s_view * s_model * vec4(vPos, 1.f);
}