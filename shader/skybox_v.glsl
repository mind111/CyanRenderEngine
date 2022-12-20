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

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vsOut.objectSpacePosition = position;
    mat4 adjustedView = view;
    // remove translation from view matrix
    adjustedView[3] = vec4(0.f, 0.f, 0.f, 1.f);
    gl_Position = (projection * adjustedView * vec4(position, 1.f)).xyww;
}