#version 450 core

layout (location = 0) in vec3 position;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out vec3 objectSpacePosition;

uniform mat4 cameraView;
uniform mat4 cameraProjection;

void main()
{
    objectSpacePosition = position;
    mat4 view = cameraView;
    view[3] = vec4(0.f, 0.f, 0.f, 1.f); // remove translation
    gl_Position = cameraProjection * cameraView * vec4(position, 1.f); 
}