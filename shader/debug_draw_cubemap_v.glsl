#version 450 core

layout (location = 0) in vec3 position;

// this is necessary since using seperable program
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out VertexShaderOutput
{
	vec3 objectSpacePosition;
} vsOut;

uniform mat4 cameraView;
uniform mat4 cameraProjection;

void main() 
{
	mat4 view = cameraView;
	view[3] = vec4(0.f, 0.f, 0.f, 1.f);
    gl_Position = cameraProjection * view * vec4(position, 1.f);
    vsOut.objectSpacePosition = position;
}
