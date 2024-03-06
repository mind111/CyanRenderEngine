#version 450 core

layout (points) in;
layout (points, max_vertices = 1) out;

in gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
} gl_in[];

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

in VSOutput
{
	vec3 ro;
	vec3 re;
	float cull;
} gsIn[];

struct ViewParameters
{
	uvec2 renderResolution;
	float aspectRatio;
	mat4 viewMatrix;
    mat4 prevFrameViewMatrix;
	mat4 projectionMatrix;
    mat4 prevFrameProjectionMatrix;
	vec3 cameraPosition;
	vec3 prevFrameCameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};
uniform ViewParameters viewParameters;

void main()
{
	if (gsIn[0].cull > .5f)
	{
		return;
	}

	// ro
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(gsIn[0].ro, 1.f);
	EmitVertex();
	EndPrimitive();

	// re
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(gsIn[0].re, 1.f);
	EmitVertex();
	EndPrimitive();
}
