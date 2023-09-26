#version 450 core

layout (location = 0) in vec3 position;

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

uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;

void main()
{
    vsOut.objectSpacePosition = position;
    mat4 viewMatrix = u_viewMatrix;
    // remove translation from view matrix
    viewMatrix[3] = vec4(0.f, 0.f, 0.f, 1.f);
    gl_Position = (u_projectionMatrix * viewMatrix * vec4(position, 1.f)).xyww;
}
