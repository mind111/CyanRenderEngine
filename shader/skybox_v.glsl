#version 450 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec3 vertexNormal;
layout (location = 2) in vec4 vertexTangent;
layout (location = 3) in vec2 textureUv_0;
layout (location = 4) in vec2 textureUv_1;

out VSOutput
{
	vec3 objectSpacePosition;
} vsOut;

#define VIEW_SSBO_BINDING 0
layout(std430, binding = VIEW_SSBO_BINDING) buffer ViewShaderStorageBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

void main()
{
    vsOut.objectSpacePosition = vertexPos;
    mat4 view = viewSsbo.view;
    // remove translation from view matrix
    view[3] = vec4(0.f, 0.f, 0.f, 1.f);
    gl_Position = (viewSsbo.projection * view * vec4(vertexPos, 1.f)).xyww;
}