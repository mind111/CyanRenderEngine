#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;

out VSOutput
{
    flat int instanceID;
    vec3 objectSpacePosition;
    flat ivec2 atlasTexCoord;
} vsOut;

struct InstanceDesc {
    mat4 transform;
    ivec2 atlasTexCoord;
    ivec2 padding;
};

layout(std430, binding = 63) buffer InstanceBuffer {
    InstanceDesc instances[];
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
    vsOut.instanceID = gl_InstanceID;
    vsOut.objectSpacePosition = vertexPos;
    vsOut.atlasTexCoord = instances[gl_InstanceID].atlasTexCoord;
}
