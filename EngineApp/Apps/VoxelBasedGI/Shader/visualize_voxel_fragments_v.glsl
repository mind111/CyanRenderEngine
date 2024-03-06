#version 450 core

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
	mat4 projectionMatrix;
	vec3 cameraPosition;
	vec3 cameraRight;
	vec3 cameraForward;
	vec3 cameraUp;
	int frameCount;
	float elapsedTime;
	float deltaTime;
};

uniform ViewParameters viewParameters;

struct VoxelFragment
{
    vec4 position; 
};

layout (std430, binding = 0) buffer VoxelFragmentBuffer {
    VoxelFragment voxelFragmentList[];
};

void main()
{
	vec3 worldSpacePosition = voxelFragmentList[gl_VertexID].position.xyz;
	gl_Position = viewParameters.projectionMatrix * viewParameters.viewMatrix * vec4(worldSpacePosition, 1.f);
	gl_PointSize = 10.f;
}
