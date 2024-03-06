#version 450 core

layout (points) in;
layout (line_strip, max_vertices = 24) out;

in gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
} gl_in[];

in VSOutput
{
	vec3 position;
	float size;
} gsIn[];

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

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

void emitVoxel(vec3 position, float extent)
{
	//   h -- g
	//  /|   /|
	// d -- c |
	// | e--|-f
	// |/   |/
	// a -- b
	vec3 va = position + vec3(-extent, -extent, extent);
	vec3 vb = position + vec3( extent, -extent, extent);
	vec3 vc = position + vec3( extent,  extent, extent);
	vec3 vd = position + vec3(-extent,  extent, extent);

	vec3 ve = position + vec3(-extent, -extent, -extent);
	vec3 vf = position + vec3( extent, -extent, -extent);
	vec3 vg = position + vec3( extent,  extent, -extent);
	vec3 vh = position + vec3(-extent,  extent, -extent);

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

void main()
{
	emitVoxel(gsIn[0].position, gsIn[0].size * .5f);
}
