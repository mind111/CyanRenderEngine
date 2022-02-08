#version 450 core
layout(location = 0) in vec3 vPos;
out vec3 vPosCS; 
uniform mat4 lightProjection;

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
uniform int cascadeIndex;
void main()
{
    mat4 model = gInstanceTransforms.models[transformIndex];
    vPosCS = (gDrawData.sunShadowProjections[cascadeIndex] * gDrawData.sunLightView * model * vec4(vPos, 1.f)).xyz;
    gl_Position = vec4(vPosCS, 1.f);
}
