#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec4 color;

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VertexShaderOutput 
{
	vec4 vertexColor;
} vsOut;

uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform float u_pointSize;

void main()
{
	gl_Position = u_projectionMatrix * u_viewMatrix * vec4(position, 1.f);
	vsOut.vertexColor = color;
	gl_PointSize = u_pointSize;
}
