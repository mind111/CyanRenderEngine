#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;

out VSOutput
{
    vec4 color;
    vec3 objectSpacePosition;
} vsOut;

struct DebugInstanceData {
    mat4 transform;
    vec4 color; 
};

layout(std430, binding = 53) buffer TransformSSBO {
    DebugInstanceData instances[];
};

layout(std430, binding = 0) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

void main() {
	mat4 model = instances[gl_InstanceID].transform;
    mat4 mvp = viewSsbo.projection * viewSsbo.view * model;
	gl_Position = mvp * vec4(vertexPos, 1.f);
    vsOut.objectSpacePosition = vertexPos;
    vsOut.color = instances[gl_InstanceID].color;
}
