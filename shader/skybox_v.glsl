#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VSOutput
{
	vec3 objectSpacePosition;
} vsOut;

layout(std430) buffer ViewBuffer
{
    mat4  view;
    mat4  projection;
    float m_ssao;
    float dummy;
} viewSsbo;

void main()
{
    vsOut.objectSpacePosition = position;
    mat4 view = viewSsbo.view;
    // remove translation from view matrix
    view[3] = vec4(0.f, 0.f, 0.f, 1.f);
    gl_Position = (viewSsbo.projection * view * vec4(position, 1.f)).xyww;
}