#version 450 core
layout (location = 0) in vec3 vPos; 
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec4 vTangent;

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

out vec3 normalWorld;
out vec3 tangentWorld;

void main()
{
    mat4 model = gInstanceTransforms.models[transformIndex];
    normalWorld = (inverse(transpose(model)) * vec4(vNormal, 0.f)).xyz;
    // todo: handedness of tangent
    tangentWorld = normalize((model * vec4(vTangent.xyz, 0.f)).xyz);
    gl_Position = gDrawData.projection * gDrawData.view * model * vec4(vPos, 1.f);
}