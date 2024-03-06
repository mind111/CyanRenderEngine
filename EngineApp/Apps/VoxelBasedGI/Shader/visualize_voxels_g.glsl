#version 450 core

layout (points) in;
layout (line_strip, max_vertices = 24) out;

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
uniform vec3 u_voxelSize;

void main()
{
	if (gsIn[0].cull > .5f)
	{
		return;
	}

	float cubeSize = u_voxelSize.x;
	float cubeExtent = cubeSize * .5f;
	vec3 cubeCenterWorldSpacePosition = gl_in[0].gl_Position.xyz;
	//   h -- g
	//  /|   /|
	// d -- c |
	// | e--|-f
	// |/   |/
	// a -- b

	vec3 va = cubeCenterWorldSpacePosition + vec3(-cubeExtent, -cubeExtent, cubeExtent);
	vec3 vb = cubeCenterWorldSpacePosition + vec3( cubeExtent, -cubeExtent, cubeExtent);
	vec3 vc = cubeCenterWorldSpacePosition + vec3( cubeExtent,  cubeExtent, cubeExtent);
	vec3 vd = cubeCenterWorldSpacePosition + vec3(-cubeExtent,  cubeExtent, cubeExtent);

	vec3 ve = cubeCenterWorldSpacePosition + vec3(-cubeExtent, -cubeExtent, -cubeExtent);
	vec3 vf = cubeCenterWorldSpacePosition + vec3( cubeExtent, -cubeExtent, -cubeExtent);
	vec3 vg = cubeCenterWorldSpacePosition + vec3( cubeExtent,  cubeExtent, -cubeExtent);
	vec3 vh = cubeCenterWorldSpacePosition + vec3(-cubeExtent,  cubeExtent, -cubeExtent);

	// face abcd
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vb, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vc, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vd, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	EndPrimitive();

	// face efgh
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(ve, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vf, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vg, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vh, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(ve, 1.f);
	EmitVertex();
	EndPrimitive();

	// face bfgc
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vb, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vf, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vg, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vc, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vb, 1.f);
	EmitVertex();
	EndPrimitive();

	// face aehd
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(ve, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vh, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(vd, 1.f);
	EmitVertex();
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(va, 1.f);
	EmitVertex();
	EndPrimitive();
}
